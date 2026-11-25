# Board files — plan for PeerChalk 0.4

Version 0.4 lets a board be saved to a file and opened from one. Files carry everything the
board needs and can later be kept in a repository. A second deliverable, `peerchalk-pdf`, turns
a board file into a PDF, as a command-line program and as a WebAssembly module, for other
tools to call.

The main goal is that a new kind of content (a stroke type, an image, a shape) needs no
separate save, load or conversion code.

## Decisions

1. **A saved board is the board's own Yrs document.** Every board is already a Yrs document.
   Peers exchange it, and this device keeps it as `boards/<id>.yrs`. Strokes, text boxes,
   columns, pages and page strokes all live in it. The file stores that document
   (`Board::encodeState`) as its content, so anything that syncs is saved, with no code for
   each type.
2. **Opening a file merges.** If this device already has the board (same board id), opening
   the file applies it to the board like a peer's changes, so nothing on either side is lost.
   Otherwise the file opens as a board of its own. The user can always choose "open as a copy",
   which gives the board a new id.
3. **Files hold content, never access.** No roles, members, access log, link secrets or
   device keys. Whoever opens a file becomes the admin of their own copy, unless it merges
   into a board they already have, where nothing about access changes.
4. **No hand-written JSON or XML as the stored content.** It would read well in diffs, but every
   type would need its own writer and reader, and merging two edited copies would become our
   problem instead of Yrs's. Formats meant for reading by people or other programs are
   exports that PeerChalk never reads back (PDF now; SVG or a JSON snapshot if ever wanted).

## File format: `.peerchalk`

A zip archive. Entries are stored without compression where compressing gains little
(`board.yrs` and PDFs) and written in a fixed order with fixed timestamps, so saving the same
board twice gives the same bytes.

```
manifest.json       written first, see below
board.yrs           the whole Yrs document, v1 update encoding
blobs/<sha256>      every file the board uses (today: the PDFs pages show), named by hash
preview.png         optional: the first page or the board's start, at most 512 px wide
```

`manifest.json`:

```json
{
  "format": "peerchalk-board",
  "formatVersion": 1,
  "schemaVersion": 1,
  "app": "0.4.0",
  "board": "6f1c…-uuid",
  "name": "Lecture 3",
  "kind": "pages",
  "saved": "2027-01-10T14:03:00Z",
  "blobs": ["9b2e…", "…"]
}
```

- `formatVersion` covers the archive layout, and `schemaVersion` covers what the Yrs document
  holds (see Schema). A reader refuses a newer `formatVersion` and opens a newer
  `schemaVersion`, keeping what it does not understand (see "Unknown entries").
- Opening checks everything: blob names must be hashes that match their contents, the manifest
  must name exactly the blobs present, and sizes are limited (the Yrs document and each blob,
  as for peers today). A file is as untrusted as a peer.
- **An unpacked form** is the same layout in a folder named `<name>.peerchalk/`, for
  repositories that would rather not store an archive. Reading accepts both forms, and saving
  keeps whichever form was opened.

## Schema: one place that says what a board holds

What the document holds is spread across `board.cpp`, `board_text.cpp` and `board_pages*.cpp`
today. A new `src/board/board_schema.hpp` gathers it, and the format code relies only on it:

- the names of the document's top-level parts (`strokes`, `textBoxes`, `layout`, `columns`,
  `pageStrokes`) and, for each, what its entries are and how they are encoded, with a link to
  the code that reads them;
- `boardSchemaVersion`, raised whenever the document gains something older versions would
  misread;
- **`boardBlobs(const Board &)`**: every blob the board depends on. It replaces direct uses
  of `BoardPages::pdfBlobs()`. A future item that uses a file (an image, an audio note) adds
  itself here, and saving, syncing and blob cleanup all call this one function.

### Adding a new kind of content

1. Store it in the Yrs document, as any new content must be for peers to see it. Saving then
   includes it with no further work.
2. If it uses files, add them to `boardBlobs`.
3. If older versions would misread it, raise `boardSchemaVersion`.
4. Add it to the test board, then run the format tests (see Tests), which generate a new
   fixture.

### Unknown entries

Today a stroke entry that cannot be decoded becomes an empty stroke in the mirror. It stays
untouched in the document, so a save writes it back. From 0.4 that is a rule for every part of
the document:
- readers leave entries they do not understand in the document;
- they never rewrite or delete them, except when the user erases them;
- they never write an unknown entry back in a different form.

The test board includes entries of an invented future kind to check this.

## Saving and opening

- `src/library/board_file.{hpp,cpp}`, the archive itself, with no Qt GUI:
  - `writeBoardFile(const Board &, const BoardFileInfo &, BlobLookup) -> QByteArray`
  - `readBoardFile(QByteArray) -> BoardFileContents`, which holds the manifest, the Yrs bytes
    and the blobs, all checked
  - plus the folder form of each.
- The zip code is small: stored and deflated entries only, read and written by our own code
  over zlib. zlib is the one new dependency. It is tiny, permissively licensed, builds for
  every target including WebAssembly (Emscripten ships it), and goes in `deps/` at a pinned
  version.
- `Workspace` gains `importBoard(BoardFileContents, ImportMode)`, where `ImportMode` is merge
  or copy, and the app gains `exportBoard(id)`.
- **Desktop:** "Save to file…" and "Open file…" in the board menu and on the welcome page, with
  native file dialogs, and `peerchalk file.peerchalk` on the command line. AppImage, Flatpak,
  Windows and macOS register the `.peerchalk` type (MIME type `application/x-peerchalk-board`).
- **Browser:** saving downloads the file and opening uploads one, through the existing
  `web_files.js`. Dropping a file on the page opens it too.

## Converting to PDF: `peerchalk-pdf`

```
peerchalk-pdf [options] <board.peerchalk | folder | -> [-o out.pdf | -o -]
  --column <id>        only this column (page boards)
  --pages <from>-<to>  only these pages of that column, counting from 1
  --version            prints the tool and newest board format versions it reads
```

- Exit codes: 0 means done, 1 means a bad command line, 2 means the file could not be read
  (not a board, damaged, or too new), and 3 means the PDF could not be made (for example a
  missing PDF). Errors go to stderr as one line, or as JSON with `--json-errors`, for programs
  that call the tool.
- **The same renderer as the app.** The drawing code in `src/ui/board_pdf*.cpp`
  (`renderBoardPdf`) uses QtGui only (QPainter, QPdfWriter, QTextDocument), not QML. It moves
  into a library, `peerchalk_export`, together with the board, math and PDFium code it
  needs. The app and the tool both link it, so a board looks the same either way.
- **Fonts:** the tool bundles the fonts text boxes use, so the output does not depend on the
  fonts of the machine it runs on.
- **Desktop program:** a `QGuiApplication` on the `offscreen` platform, so it runs with no
  display, including on servers and in CI. It ships beside `peerchalk` in every desktop
  package and as its own small archive for each platform in the GitHub release.
- **WebAssembly module:** `peerchalk-pdf.mjs` and `.wasm`, built from the same code with Qt for
  WebAssembly:

  ```js
  import { loadConverter } from './peerchalk-pdf.mjs';
  const convert = await loadConverter();                // fetches the .wasm and fonts
  const pdf = await convert(boardBytes, { column, from, to }); // Uint8Array
  ```

  Formulas are laid out by the math module, as in the browser app. Released as an archive on
  GitHub; npm later if wanted.

### Limit to settle early: where the WebAssembly module can run

Qt for WebAssembly needs a web page: it does not run in Node.js or in a Web Worker. The app
already exports PDFs in browsers this way, so a browser module is safe. A module for Node.js
or servers is not: it would need text layout without Qt, which is the part most likely to look
different from the app. The plan is:

1. **0.4:** the desktop program everywhere, and the module for web pages. Programs on servers
   use the desktop program.
2. **Later, only if it is needed:** a renderer without Qt for Node.js and workers. PDFium
   draws the paths, strokes and formulas, which are already triangles. Text would need HarfBuzz
   for shaping and our own line breaking, with tests comparing its PDFs against the app's
   renderer page by page.

## Repository sync (after 0.4)

Yrs data does not diff well as text, but it merges without conflicts, which matters more here.

- **Git merge driver:** `peerchalk-pdf` has a sibling, `peerchalk-board merge <base> <ours>
  <theirs>`. It applies both sides' Yrs documents to one board and writes the result, so two
  people editing the same board offline never get a merge conflict. Set it up with a line in
  `.gitattributes` (`*.peerchalk merge=peerchalk`) and in `git config`. The same program can
  offer `peerchalk-board info` (prints the manifest) and `peerchalk-board check` (validates a
  file).
- **Watched files:** a board opened from a file can stay linked to it. PeerChalk saves to the
  file after changes, and when the file changes on disk (git pull, Syncthing, Dropbox) it
  merges the new contents in, the way it merges a peer's changes. This needs a file watcher
  (`QFileSystemWatcher` on desktops; browsers cannot watch files) and the merge path above.
- Linking a whole repository, where every board file in a folder appears as a project, builds on
  both and is left for later.

## Tests

- **Round trip:** save the test board, open the file into a new board, and require the same
  Yrs document and the same blobs. Then save again and require the same bytes.
- **Test board:** one board built in code with every kind of content: pen and highlighter
  strokes, text boxes with formulas, an infinite board and a page board with columns,
  subheadings, PDF pages and page strokes, plus entries of an invented future kind.
- **Fixtures:** `tests/fixtures/boards/schema-<n>.peerchalk`, one for each schema version, kept
  forever. Every version must open every older fixture and keep its contents. A test fails when
  `boardSchemaVersion` is raised without a fixture for the new version.
- **Untrusted files:** truncated and altered archives, blob hashes that do not match, missing
  or extra blobs, oversized parts, a newer `formatVersion`, and zip paths with `..` or absolute
  paths. Each must be refused with a clear error and no crash.
- **Merge:** two copies edited apart and then merged by opening and by the git driver must give
  the same board as live sync would.
- **`peerchalk-pdf`:**
  - the PDF of the test board matches `renderBoardPdf` from the app, compared by rendering
    both with PDFium;
  - the exit codes and `-` for stdin and stdout work;
  - the desktop program runs on Linux, macOS and Windows in CI, and the module runs in headless
    Chromium in the browser CI job.

## Milestones

1. **Schema:** `board_schema.hpp`, `boardBlobs`, the test board, and the rule for unknown
   entries.
2. **Format:** `board_file` with both forms, its tests, and the first fixture.
3. **App:** saving and opening on desktop and in the browser, merge or copy, and file
   associations.
4. **Converter:** the `peerchalk_export` library, the desktop `peerchalk-pdf`, and the
   WebAssembly module, each with packaging and CI.
5. **Release 0.4.**
6. **After 0.4:** `peerchalk-board merge` and the git setup, then watched files.

## Risks

- **Yrs encoding:** the v1 update encoding is Yjs's documented wire format, and our pinned
  Yrs already depends on it for sync, so files are no riskier than peers. Fixtures catch a
  change.
- **File size:** the document keeps a small marker for each deleted item, so a board with
  years of erasing grows slowly. If that ever matters, "save compacted" can write a fresh
  document holding only the current content. A compacted file merges as a new copy.
- **Fonts and layout:** the PDF depends on the bundled fonts and Qt's layout, so the tool
  pins both with the app.
