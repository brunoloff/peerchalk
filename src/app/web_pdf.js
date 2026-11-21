// Linked into the browser build with --pre-js: reaches PDFium's own WebAssembly module, which
// the page loads beside the app (deploy/web/site/loader.js puts it in
// globalThis.peerchalkPdfium). src/pdf/pdfium_web_bridge.cpp calls these, synchronously, to
// copy what PDFium reads and writes between its memory and the app's.
const peerchalkPdf = {
  // The PDFs open in PDFium, by document: where their bytes are in PDFium's memory.
  documents: new Map(),
  // What the last save wrote, in pieces, until the app takes it.
  savedChunks: null,

  module() {
    return globalThis.peerchalkPdfium;
  },

  // Copies `size` bytes of the app's memory at `from` into PDFium's, which the caller frees.
  copyIn(from, size) {
    const P = this.module();
    const to = P._malloc(Math.max(1, size));
    P.HEAPU8.set(HEAPU8.subarray(from, from + size), to);
    return to;
  },

  copyOut(from, to, size) {
    HEAPU8.set(this.module().HEAPU8.subarray(from, from + size), to);
  },

  // A text getter of PDFium's (FPDF_GetMetaText, FPDFBookmark_GetTitle) into the app's buffer.
  text(call, buffer, length) {
    const P = this.module();
    const inside = length > 0 ? P._malloc(length) : 0;
    const written = call(inside, length);
    if (inside) {
      this.copyOut(inside, buffer, Math.min(written, length));
      P._free(inside);
    }
    return written;
  },

  // Floats PDFium writes through pointers, copied to the app's pointers.
  floats(call, ...targets) {
    const P = this.module();
    const inside = P._malloc(4 * targets.length);
    const result = call(...targets.map((_, i) => inside + 4 * i));
    targets.forEach((target, i) => {
      if (target) this.copyOut(inside + 4 * i, target, 4);
    });
    P._free(inside);
    return result;
  },

  present() {
    return this.module() ? 1 : 0;
  },

  load(data, size) {
    const P = this.module();
    if (!P) return 0;
    const inside = this.copyIn(data, size);
    const document = P._FPDF_LoadMemDocument64(inside, size, 0);
    if (document) this.documents.set(document, inside);
    else P._free(inside);
    return document;
  },

  close(document) {
    const P = this.module();
    P._FPDF_CloseDocument(document);
    const inside = this.documents.get(document);
    if (inside !== undefined) P._free(inside);
    this.documents.delete(document);
  },

  call(name, ...args) {
    return this.module()["_" + name](...args);
  },

  pageSize(document, page, size) {
    return this.floats((width) => this.module()._FPDF_GetPageSizeByIndexF(document, page, width),
                       size, size + 4);
  },

  metaText(document, tag, buffer, length) {
    const P = this.module();
    const name = new TextEncoder().encode(tag);
    const inside = P._malloc(name.length + 1);
    P.HEAPU8.set(name, inside);
    P.HEAPU8[inside + name.length] = 0;
    const written = this.text((to, size) => P._FPDF_GetMetaText(document, inside, to, size),
                              buffer, length);
    P._free(inside);
    return written;
  },

  bookmarkTitle(bookmark, buffer, length) {
    return this.text((to, size) => this.module()._FPDFBookmark_GetTitle(bookmark, to, size),
                     buffer, length);
  },

  bitmapCopy(bitmap, to, size) {
    this.copyOut(this.module()._FPDFBitmap_GetBuffer(bitmap), to, size);
  },

  importPages(dest, source, pages, count, index) {
    const P = this.module();
    const inside = this.copyIn(pages, 4 * count);
    const ok = P._FPDF_ImportPagesByIndex(dest, source, inside, count, index);
    P._free(inside);
    return ok;
  },

  box(name, page, left, bottom, right, top) {
    const getter = this.module()["_" + name];
    return this.floats((l, b, r, t) => getter(page, l, b, r, t), left, bottom, right, top);
  },

  save(document, flags) {
    const P = this.module();
    const chunks = [];
    const write = P.addFunction((self, data, size) => {
      chunks.push(P.HEAPU8.slice(data, data + size));
      return 1;
    }, "iiii");
    const writer = P._malloc(8); // FPDF_FILEWRITE: version, WriteBlock
    P.HEAP32[writer >> 2] = 1;
    P.HEAP32[(writer >> 2) + 1] = write;
    const ok = P._FPDF_SaveAsCopy(document, writer, flags);
    P._free(writer);
    P.removeFunction(write);
    this.savedChunks = ok ? chunks : null;
    return ok ? chunks.reduce((total, chunk) => total + chunk.length, 0) : -1;
  },

  takeSaved(to) {
    let at = to;
    for (const chunk of this.savedChunks ?? []) {
      HEAPU8.set(chunk, at);
      at += chunk.length;
    }
    this.savedChunks = null;
  },
};
