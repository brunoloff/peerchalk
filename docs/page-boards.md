# Page boards — plan for PeerChalk 0.2

PeerChalk 0.1 has one kind of board: an infinite canvas. Version 0.2 adds a second kind, the
**page board**: a row of columns, each a stack of pages of a real paper size, with headings.
PDFs can be imported as a column of pages to write on. Hundreds of pages must stay smooth.

Interface designs: the "PeerChalk page boards" design canvas
(https://claude.ai/artifact/GaZpHfr473poFsst736NbN).

## Terms

- **Column**: a vertical stack of items. It has a paper size: a page width and a default page
  height. Columns sit left to right with a fixed gap; there is a first column and any number to
  its right.
- **Item**: a page or a subheading (h2), in column order.
- **Page**: a sheet with a background (blank, ruled, squared, dotted, or a PDF page) and the
  strokes and text boxes drawn on it. A page normally has its column's size; a PDF page keeps
  the size the PDF gives it.
- **Heading (h1)**: one per column, above its first item. **Subheading (h2)**: an item
  between pages, or right below the heading.

Units stay as they are: 1 board unit = 1/96 inch, so 1 pt = 4/3 units. A4 is
793.70 × 1122.52 units (595.28 × 841.89 pt) and Letter is 816 × 1056 units (612 × 792 pt).
Sizes are stored as floats and written to PDFs in points, so an exported A4 page is exactly A4.

## Decisions (recommendations; confirm before M1)

1. **PDF engine: PDFium on every platform**, from prebuilt `pdfium-binaries` (BSD-3 / Apache-2.0
   with permissive third-party parts, fine for MIT and for static WebAssembly). Qt PDF is also
   PDFium, but Qt does not ship it for WebAssembly, and it exposes no page-composition API,
   which export needs (see Export). PDFium's progressive rendering (`FPDF_RenderPageBitmap_Start`
   / `_Continue`) lets the single-threaded browser build render in time slices.
   Fallback if the wasm build disappoints in M0: pdf.js (Apache-2.0) in a web worker for the
   browser, the same way `web_math.js` bridges to JavaScript today.
2. **A board's kind is fixed at creation.** An infinite board does not turn into a page board.
3. **Strokes belong to pages** and are stored in page coordinates, so inserting a page above
   moves everything below with its page. A stroke belongs to the page it starts on and is
   clipped to that page, like ink on paper. The gaps between pages are for the + controls.
   Text boxes also belong to a page.
4. **PDF files travel separately from the Yrs document**, as content-addressed blobs (SHA-256),
   so a board's document stays small and a PDF shared by several boards is sent once.
5. **Older versions never see page boards.** Protocol version 4 is also the oldest one
   accepted: before 1.0 there is no backwards compatibility, so 0.1 devices are told to
   update instead of showing a page board as an empty infinite board.
6. **Changing a column's paper size** changes its width and the default for new pages. Existing
   pages keep their size unless the person picks "Apply to all pages in this column". PDF pages
   are never resized.

## Data model (Yrs)

A page board's document holds, beside the existing branches:

```
layout:  YMap   { kind: "pages", version: 1 }
columns: YArray of YMap {
           id, title (h1 text), paperWidth, paperHeight, background,
           items: YArray of YMap {
             id, type: "page" | "heading",
             text                       (headings)
             width?, height?            (pages; absent = the column's paper)
             background?                (pages; absent = the column's background;
                                         { kind: "pdf", blob: <sha256>, page: <index>,
                                           width, height } for PDF pages)
           }
         }
pageStrokes: YMap  pageId -> YArray of encodeStroke() binaries (page coordinates)
```

- Strokes are kept by page id, not inside the item, so moving a page (a delete plus an insert in
  Yrs) never copies its strokes, and decoding happens one page at a time.
- Text boxes on pages stay in the existing `textBoxes` map, with a `page` key holding their
  page's id and their position in page coordinates, so all of the text editing is shared.
- Removing a page also removes its `pageStrokes` entry and its text boxes, in the same undo
  step.
- Concurrent edits merge as CRDT arrays do: two people adding a page at the same place both get
  a page, and the order is the same on every device.
- `Board` grows a `PageBoard` sibling (`src/board/page_board.{hpp,cpp}`) that owns these
  branches. The stroke codec, eraser and undo manager are shared; the undo manager also tracks
  the new branches.

## Layout

A pure function, `PageLayout layoutPages(const PageBoard&)` in `src/board/page_layout.{hpp,cpp}`,
turns the model into world rectangles:

- Column x positions: the sum of earlier column widths plus a 96-unit (1 inch) gap. A column's
  width is the widest of its paper width and its pages.
- Within a column: the h1 band (fixed height), then the items with a 32-unit gap. Pages narrower
  than the column are centred.
- Output: per page, its world rectangle; per heading, its rectangle; and the insertion slots
  between items and between columns, which the interface turns into + buttons.

Layout is O(pages) and only reruns when the column structure changes, not on strokes. Lookups
(which page is under the pointer, which pages are visible) binary-search a column's sorted y
positions. Unit tests cover sizes, gaps, mixed-width PDF columns and slot positions.

## Rendering and performance

### Today

- `StrokeLayer` rebuilds one scene-graph node for **every stroke on the board** whenever
  anything changes (`setStrokeNodes`), and it does no culling.
- The text box layer instantiates a QML delegate for every text box.
- The minimap draws every stroke again.

That is fine for one whiteboard and does not scale to hundreds of pages.

### Page canvas

The page canvas is a `BoardCanvas` sibling (`PageCanvas`) that shares its input handling, tools
and drafts, but draws page by page:

1. **Culling.** Only pages within the viewport plus one screen of margin get scene nodes. Pages
   that leave are released, with a small LRU kept for quick returns.
2. **One mesh per page per layer.** Each page's strokes are merged into one `QSGGeometry` with
   per-vertex colour (`QSGVertexColorMaterial`), separately for pen and highlighter (the
   highlighter layer keeps today's opacity trick). That is two draw calls per page, not one per
   stroke. A new stroke is appended to its page's mesh; an erase rebuilds only that page.
3. **Level of detail.** Below about 160 px of screen height, a page shows a cached low-resolution
   texture of its ink instead of vectors. When the whole board is far out, every page is a plain
   white rectangle, all in one node.
4. **Lazy decoding.** Strokes are decoded the first time their page comes into view.
5. **Text boxes.** The QML model holds only boxes on visible pages.
6. **Navigator.** The minimap shows page rectangles and headings, one node, no strokes.

### PDF backgrounds

- **Render queue.** All PDFium calls run on one worker thread natively; PDFium is not
  thread-safe. In the browser they run as progressive renders on the main thread, a few
  milliseconds per frame. Requests are ordered by visibility and then by distance from the view
  centre, and are cancelled when their page scrolls away.
- **Resolution buckets.** A page renders at its on-screen pixel size rounded up to a power of √2.
  While a sharper bucket renders, the nearest cached one is shown stretched, so nothing is ever
  blank after the first paint; before that, a page shows paper colour with faint text lines.
- **Tiles.** Above 2048 device pixels on screen, a page renders as 512-pixel tiles, only the
  visible ones.
- **Cache.** Textures are kept in an LRU with a byte budget: 384 MB native and 128 MB in
  browsers, lowered on phones. PDF documents stay open (`FPDF_LoadCustomDocument` reading from
  the blob store); pages are loaded and closed as needed.

### Targets and measurements

A benchmark (`tests/page_board_bench.cpp`, offscreen) builds two synthetic boards: 500 pages
with 200 strokes each, and a 300-page PDF. It measures:

- time to first frame
- frame time while panning and zooming (p95)
- the texture cache's peak memory

Targets on a mid-range laptop:

- first visible page of a 300-page PDF in under 1 s
- 60 fps panning at 100 % and fully zoomed out
- the whole 500-page board flung through without stalls

The benchmark reports in CI without failing the build until the targets are met.

The same per-region approach can later make large infinite boards fast, with spatial tiles in
place of pages. That is outside 0.2.

## PDF import, storage and sync

- **Import.** "Import PDF" on the board, or in a column's menu, opens the system picker (the
  browser picker on the web), then:
  1. hashes the file and stores it as a blob
  2. reads its page count and sizes with PDFium
  3. adds a column to the right of the current one, holding one page per PDF page, with the
     PDF's title (or its file name) as the heading. The column's paper is the most common page
     size.

  Large files show progress. Encrypted or broken PDFs are refused with a clear message.
- **Blob store.** Natively, blobs live in `<account>/blobs/<sha256>`; in the browser, in
  IndexedDB (beside `web_storage.js`). A blob that no board references for 30 days is deleted.
- **Sync.**
  - Messages `BlobWant { hash, offset }` and `BlobChunk { hash, offset, total, data }`, in
    chunks of at most 1 MiB, with up to 4 requests in flight per peer.
  - A peer answers only when the hash is referenced by a board the asker may access.
  - The receiver checks the SHA-256 before storing.
  - Limit: 200 MB per file.
  - Pages whose PDF is still arriving show "Fetching PDF · 34 %" and can already be written on.
- **Protocol** becomes 4. Additions are ignored by peers that don't know them, and decision 5
  keeps page boards away from older peers.

## Export

- One PDF page per board page, at its exact size. The h1 and h2 headings become the PDF outline
  (bookmarks), so a 300-page export stays navigable.
- **PDF pages keep their vector content.** The annotations are drawn with the existing
  `QPdfWriter` path into a temporary PDF. PDFium then composes each output page: the original
  page as a form XObject (`FPDF_NewXObjectFromPage`), with the annotation page on top, saved
  with `FPDF_SaveAsCopy`. Text stays selectable and files stay small.
- Choices: the whole board, one column, or a page range.

## Interface (see the design canvas)

- **Column ruler.** A strip pinned to the top of the board shows each column's h1, aligned with
  the column below as you pan. It stays visible far down a long column. It has a + between
  neighbouring headings and one after the last, and a column menu (paper size, background,
  import PDF, delete).
- **In the board.** The h1 is printed above its column's first page. Hovering a gap between
  pages shows a small "+ Page · + Subheading" pill. An h2 sits in the gap like a section title.
  Page numbers sit under each page, and the column's last gap holds the larger "Add page" button.
- **Inspector**, on the right as today. With a page or column selected it shows paper size
  (A4, A5, Letter, Legal, custom in mm or inches), background and orientation. On PDF pages it
  shows the source file.
- **Outline.** The navigator gains an outline tab: columns, their subheadings and page counts.
  Clicking jumps there.
- **New board.** The dialog asks for the kind first (Infinite / Pages), then, for pages, paper
  size and background.
- **Board card.** Says "Pages · 3 columns · 142 pages" in place of "Infinite board".
- **Phones.** The ruler becomes a column picker; + pills are always visible, since there is no
  hover.

## Milestones

Each milestone lands on main with tests, and keeps infinite boards working.

| | Milestone | Done when |
|---|---|---|
| M0 | Spikes | PDFium links and renders on all six targets including wasm (size, speed, progressive rendering measured); nested Yrs arrays and undo across them confirmed in libyrs |
| M1 | Model and layout | `PageBoard`, `layoutPages`, saving and loading, merges between two replicas, unit tests |
| M2 | Page canvas | Culled per-page meshes, drawing, erasing, text and undo on pages; the benchmark exists |
| M3 | Page interface | New-board dialog, column ruler, + controls, headings, inspector paper settings, outline |
| M4 | PDF display and import | Render queue, buckets, tiles, cache, import flow; the 300-page target met |
| M5 | Sync | Blob store and messages, protocol 4 (older versions refused), progress on pages |
| M6 | Export | Exact page sizes, vector-preserving composition, bookmarks, ranges |
| M7 | Polish and release | Phone layout, benchmark targets met, metainfo and release notes, 0.2 |

## Risks

- **Browser memory and size.** PDFium adds several MB to the wasm download, and big PDFs press
  on the browser's memory. M0 measures both; pdf.js in a worker is the fallback.
- **Windows.** `pdfium-binaries` ships MSVC-built DLLs. Their C interface should load from the
  MinGW build, which M0 confirms.
- **Yrs details.** Undo across nested arrays, and keeping a page's strokes when someone else
  deletes the page at the same time (the page wins: strokes whose page is gone are dropped
  from view but kept until the deletion is undone or forgotten).
- **Notices.** PDFium's third-party licences (FreeType, libjpeg-turbo, OpenJPEG, Little CMS,
  zlib) join the generated notices for every package.

## Progress

Work happens on the `page-boards` branch, one commit per finished piece.

- [x] Plan and interface designs
- [x] M1 · `layoutPages`, paper sizes, `itemsIn` and `pageAt` (`src/board/page_layout.*`, 7 tests)
- [x] M1 · `BoardPages` (`board.pages()`, `src/board/board_pages*`, 9 tests): kind and
      `start`, columns and items by id, strokes per page decoded lazily, undo, merges between
      two replicas, saving and loading
- [x] M1 · moving items and columns, text boxes on pages (`TextBox::page`), per-page sizes
      and "Apply to all pages" (`setColumnPaper(..., allPages)`, `setPageSize`); 14 tests
      Notes from libyrs (`libyrs.h`): nested types go in with `yinput_yarray` / `yinput_ymap`
      and come out with `youtput_read_yarray` / `youtput_read_ymap`; `yobserve_deep` reports
      nested changes; an undo scope on a root branch covers what is nested inside it. There
      is no array move, so moving a page is a delete plus an insert, which is why strokes and
      text boxes are keyed by page id outside the item.
- [x] M1 · `Board` knows its kind (`layout` map); the library shows "Pages · N columns"
- [x] M2 · page canvas: `BoardCanvas` shows sheets for page boards; drawing, erasing and
      text go to the page under the pointer in page coordinates; `PageStrokeNodes` keeps one
      merged, clipped mesh per page in view and layer, rebuilt only on change; strokes drawn
      with fewer points below 50 % (`simplifiedStroke`); `PageMap` lookups
- [x] M2 · benchmark `tests/page_board_bench.cpp` (`peerchalk_page_bench`, not in ctest).
      First run, scene-node work only (not GPU time), 500 pages × 200 strokes of 30 points:
      build 0.5 s, save 37.8 MB, layout 1.1 ms; panning a 1920 × 1080 view p95 per frame:
      5.2 ms at 100 %, 2.9 ms at 25 %, 3.9 ms at 2 %; the worst single frame (first decode)
      15 ms at 100 % and 46 ms at 2 %
- [x] M3 · new-board dialog (kind, paper, orientation, background); page backgrounds (blank,
      ruled, squared, dotted) drawn on the sheets; a sticky column ruler that sits on the
      heading band and pins below the title, with a column menu (rename, paper for new pages
      or all pages, background, add left/right, move, delete) and + between columns; "+ Page ·
      + Subheading" in the gaps and "+ Add page" at each column's end; subheadings with rename
      and remove; page numbers; the minimap shows pages; boards open on their first column.
      The printed h1 is left to the ruler on screen (export will print it); an outline of
      columns and subheadings in the side panel jumps to them
- [x] M0 · PDFium on Linux: pdfium-binaries chromium/8125 through the dependency build
      (`deps/CMakeLists.txt`); builds without it get `pdf_document_none.cpp` and hide import
- [ ] M0 · PDFium for Windows, macOS and the browser, checked in CI (needs a push, so ask
      first)
- [x] M4 · PDF display and import (Linux): `PdfDocument` (PDFium behind one lock),
      `BlobStore` (`<account>/blobs/<sha256>`), `BoardPages::addPdfColumn` (PDF pages keep
      their size), `Library::importPdf` (200 MB limit, clear refusals), `PdfRenderer` (one
      worker thread, √2 size buckets from 64 to 4096 px, 384 MB / 128 MB image budget, the
      view's wishes replace the queue), textures held by the canvas's root scene node;
      "Import a PDF…" on the ruler's + and in the column menu
- [x] M5 · sync: protocol 4, also the oldest accepted (no compatibility before 1.0);
      `BlobWant` / `BlobChunk` in 256 KiB pieces, one request at a time from one peer,
      answered only for blobs on boards shared with the asker, stored only when the SHA-256
      matches; an end-to-end test sends a 700 KB PDF between two devices
- [ ] M5 · still to do: "Fetching PDF · 34 %" on pages, a second peer when the first leaves
      mid-download (it retries on the next connection now), forgetting unused blobs
- [x] M6 · export: one PDF page per board page, column by column, at its exact size (QPdfWriter
      rounds to whole points, so PDFium sets each page's box); ink and text boxes clipped to
      their page; PDF pages keep the original page as it is (`composePdf`: imported page with
      the ink as a form object over it, rotation and offsets handled); a missing PDF refuses
      the export with a clear message
- [ ] M6 · still to do: headings as bookmarks (PDFium cannot write outlines; needs a small
      writer of our own), exporting one column or a page range, printing the h1
- [ ] M4 · still to do: tiles above 4096 px, progressive rendering in the browser, the
      "Fetching PDF" placeholder, the 300-page timing
