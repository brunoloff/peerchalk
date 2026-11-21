// PDFium in the browser: the page loads PDFium's own WebAssembly module beside the app
// (pdfium-binaries' build, the same engine as on desktops). This file defines the PDFium
// functions that pdf_document.cpp and pdf_compose.cpp call, each forwarding to that module
// through src/app/web_pdf.js, so those files are the same on every platform. Handles are
// PDFium's own pointers, passed through as they are; buffers are copied between the two
// modules' memories.
#include "pdf/pdf_engine.hpp"

#include <fpdf_doc.h>
#include <fpdf_edit.h>
#include <fpdf_ppo.h>
#include <fpdf_save.h>
#include <fpdf_transformpage.h>
#include <fpdfview.h>

#include <emscripten/em_js.h>

#include <cstdint>
#include <map>
#include <vector>

namespace {

EM_JS_DEPS(peerchalk_pdfium, "$UTF8ToString");

template <typename Handle> Handle handle(int value) {
    return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(static_cast<unsigned>(value)));
}

int raw(const void *pointer) { return static_cast<int>(reinterpret_cast<std::uintptr_t>(pointer)); }

// What FPDFBitmap_GetBuffer hands out: a copy, in the app's memory, of a bitmap's pixels.
std::map<int, std::vector<unsigned char>> bitmapCopies;

// Each forwards to src/app/web_pdf.js. The bodies stay single calls: the preprocessor reads
// them, and would mangle JavaScript operators such as !==.
EM_JS(int, pdfium_present, (), { return peerchalkPdf.present(); });
EM_JS(void, pdfium_init, (), { peerchalkPdf.call("FPDF_InitLibrary"); });
EM_JS(int, pdfium_load, (const void *data, int size), { return peerchalkPdf.load(data, size); });
EM_JS(void, pdfium_close, (int document), { peerchalkPdf.close(document); });
EM_JS(int, pdfium_call, (const char *name, int a, int b, int c),
      { return peerchalkPdf.call(UTF8ToString(name), a, b, c); });
EM_JS(double, pdfium_call_float, (const char *name, int a),
      { return peerchalkPdf.call(UTF8ToString(name), a); });
EM_JS(int, pdfium_size, (int document, int page, float *size),
      { return peerchalkPdf.pageSize(document, page, size); });
EM_JS(int, pdfium_meta_text, (int document, const char *tag, void *buffer, int length),
      { return peerchalkPdf.metaText(document, UTF8ToString(tag), buffer, length); });
EM_JS(int, pdfium_bookmark_title, (int bookmark, void *buffer, int length),
      { return peerchalkPdf.bookmarkTitle(bookmark, buffer, length); });
EM_JS(void, pdfium_render, (int bitmap, int page, int x, int y, int width, int height, int flags),
      { peerchalkPdf.call("FPDF_RenderPageBitmap", bitmap, page, x, y, width, height, 0, flags); });
EM_JS(void, pdfium_fill, (int bitmap, int left, int top, int width, int height, unsigned colour),
      { peerchalkPdf.call("FPDFBitmap_FillRect", bitmap, left, top, width, height, colour); });
EM_JS(void, pdfium_bitmap_copy, (int bitmap, void *to, int size),
      { peerchalkPdf.bitmapCopy(bitmap, to, size); });
EM_JS(int, pdfium_import, (int dest, int source, const int *pages, int count, int index),
      { return peerchalkPdf.importPages(dest, source, pages, count, index); });
EM_JS(void, pdfium_transform,
      (int object, double a, double b, double c, double d, double e, double f),
      { peerchalkPdf.call("FPDFPageObj_Transform", object, a, b, c, d, e, f); });
EM_JS(int, pdfium_box, (const char *name, int page, float *l, float *b, float *r, float *t),
      { return peerchalkPdf.box(UTF8ToString(name), page, l, b, r, t); });
EM_JS(void, pdfium_set_box, (const char *name, int page, double l, double b, double r, double t),
      { peerchalkPdf.call(UTF8ToString(name), page, l, b, r, t); });
EM_JS(int, pdfium_save, (int document, int flags), { return peerchalkPdf.save(document, flags); });
EM_JS(void, pdfium_take_saved, (void *to), { peerchalkPdf.takeSaved(to); });

int call(const char *name, int a = 0, int b = 0, int c = 0) { return pdfium_call(name, a, b, c); }

} // namespace

namespace peerchalk::pdf_engine {

bool present() { return pdfium_present() != 0; }

} // namespace peerchalk::pdf_engine

extern "C" {

FPDF_EXPORT void FPDF_CALLCONV FPDF_InitLibraryWithConfig(const FPDF_LIBRARY_CONFIG *) {
    if (pdfium_present())
        pdfium_init();
}

FPDF_EXPORT FPDF_DOCUMENT FPDF_CALLCONV FPDF_LoadMemDocument64(const void *data, size_t size,
                                                               FPDF_BYTESTRING) {
    return handle<FPDF_DOCUMENT>(pdfium_load(data, static_cast<int>(size)));
}

FPDF_EXPORT void FPDF_CALLCONV FPDF_CloseDocument(FPDF_DOCUMENT document) {
    if (document != nullptr)
        pdfium_close(raw(document));
}

FPDF_EXPORT unsigned long FPDF_CALLCONV FPDF_GetLastError() {
    return pdfium_present() ? static_cast<unsigned long>(call("FPDF_GetLastError")) : 1UL;
}

FPDF_EXPORT int FPDF_CALLCONV FPDF_GetPageCount(FPDF_DOCUMENT document) {
    return call("FPDF_GetPageCount", raw(document));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDF_GetPageSizeByIndexF(FPDF_DOCUMENT document, int index,
                                                             FS_SIZEF *size) {
    return pdfium_size(raw(document), index, &size->width);
}

FPDF_EXPORT unsigned long FPDF_CALLCONV FPDF_GetMetaText(FPDF_DOCUMENT document,
                                                         FPDF_BYTESTRING tag, void *buffer,
                                                         unsigned long length) {
    return static_cast<unsigned long>(
        pdfium_meta_text(raw(document), tag, buffer, static_cast<int>(length)));
}

FPDF_EXPORT FPDF_PAGE FPDF_CALLCONV FPDF_LoadPage(FPDF_DOCUMENT document, int index) {
    return handle<FPDF_PAGE>(call("FPDF_LoadPage", raw(document), index));
}

FPDF_EXPORT void FPDF_CALLCONV FPDF_ClosePage(FPDF_PAGE page) { call("FPDF_ClosePage", raw(page)); }

FPDF_EXPORT FPDF_BITMAP FPDF_CALLCONV FPDFBitmap_Create(int width, int height, int alpha) {
    return handle<FPDF_BITMAP>(call("FPDFBitmap_Create", width, height, alpha));
}

FPDF_EXPORT void FPDF_CALLCONV FPDFBitmap_Destroy(FPDF_BITMAP bitmap) {
    bitmapCopies.erase(raw(bitmap));
    call("FPDFBitmap_Destroy", raw(bitmap));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDFBitmap_FillRect(FPDF_BITMAP bitmap, int left, int top,
                                                        int width, int height, FPDF_DWORD colour) {
    pdfium_fill(raw(bitmap), left, top, width, height, static_cast<unsigned>(colour));
    return 1;
}

FPDF_EXPORT int FPDF_CALLCONV FPDFBitmap_GetStride(FPDF_BITMAP bitmap) {
    return call("FPDFBitmap_GetStride", raw(bitmap));
}

// Called once the bitmap is drawn: copies its pixels into the app's memory.
FPDF_EXPORT void *FPDF_CALLCONV FPDFBitmap_GetBuffer(FPDF_BITMAP bitmap) {
    const int size = FPDFBitmap_GetStride(bitmap) * call("FPDFBitmap_GetHeight", raw(bitmap));
    std::vector<unsigned char> &copy = bitmapCopies[raw(bitmap)];
    copy.resize(static_cast<std::size_t>(size));
    pdfium_bitmap_copy(raw(bitmap), copy.data(), size);
    return copy.data();
}

FPDF_EXPORT void FPDF_CALLCONV FPDF_RenderPageBitmap(FPDF_BITMAP bitmap, FPDF_PAGE page, int x,
                                                     int y, int width, int height, int, int flags) {
    pdfium_render(raw(bitmap), raw(page), x, y, width, height, flags);
}

FPDF_EXPORT FPDF_BOOKMARK FPDF_CALLCONV FPDFBookmark_GetFirstChild(FPDF_DOCUMENT document,
                                                                   FPDF_BOOKMARK bookmark) {
    return handle<FPDF_BOOKMARK>(call("FPDFBookmark_GetFirstChild", raw(document), raw(bookmark)));
}

FPDF_EXPORT FPDF_BOOKMARK FPDF_CALLCONV FPDFBookmark_GetNextSibling(FPDF_DOCUMENT document,
                                                                    FPDF_BOOKMARK bookmark) {
    return handle<FPDF_BOOKMARK>(call("FPDFBookmark_GetNextSibling", raw(document), raw(bookmark)));
}

FPDF_EXPORT unsigned long FPDF_CALLCONV FPDFBookmark_GetTitle(FPDF_BOOKMARK bookmark, void *buffer,
                                                              unsigned long length) {
    return static_cast<unsigned long>(
        pdfium_bookmark_title(raw(bookmark), buffer, static_cast<int>(length)));
}

FPDF_EXPORT FPDF_DEST FPDF_CALLCONV FPDFBookmark_GetDest(FPDF_DOCUMENT document,
                                                         FPDF_BOOKMARK bookmark) {
    return handle<FPDF_DEST>(call("FPDFBookmark_GetDest", raw(document), raw(bookmark)));
}

FPDF_EXPORT int FPDF_CALLCONV FPDFDest_GetDestPageIndex(FPDF_DOCUMENT document, FPDF_DEST dest) {
    return call("FPDFDest_GetDestPageIndex", raw(document), raw(dest));
}

FPDF_EXPORT FPDF_DOCUMENT FPDF_CALLCONV FPDF_CreateNewDocument() {
    return handle<FPDF_DOCUMENT>(call("FPDF_CreateNewDocument"));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDF_ImportPagesByIndex(FPDF_DOCUMENT dest,
                                                            FPDF_DOCUMENT source, const int *pages,
                                                            unsigned long count, int index) {
    return pdfium_import(raw(dest), raw(source), pages, static_cast<int>(count), index);
}

FPDF_EXPORT FPDF_XOBJECT FPDF_CALLCONV FPDF_NewXObjectFromPage(FPDF_DOCUMENT dest,
                                                               FPDF_DOCUMENT source, int index) {
    return handle<FPDF_XOBJECT>(call("FPDF_NewXObjectFromPage", raw(dest), raw(source), index));
}

FPDF_EXPORT void FPDF_CALLCONV FPDF_CloseXObject(FPDF_XOBJECT form) {
    call("FPDF_CloseXObject", raw(form));
}

FPDF_EXPORT FPDF_PAGEOBJECT FPDF_CALLCONV FPDF_NewFormObjectFromXObject(FPDF_XOBJECT form) {
    return handle<FPDF_PAGEOBJECT>(call("FPDF_NewFormObjectFromXObject", raw(form)));
}

FPDF_EXPORT void FPDF_CALLCONV FPDFPageObj_Transform(FPDF_PAGEOBJECT object, double a, double b,
                                                     double c, double d, double e, double f) {
    pdfium_transform(raw(object), a, b, c, d, e, f);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDFPage_InsertObject(FPDF_PAGE page, FPDF_PAGEOBJECT object) {
    return call("FPDFPage_InsertObject", raw(page), raw(object));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDFPage_GenerateContent(FPDF_PAGE page) {
    return call("FPDFPage_GenerateContent", raw(page));
}

FPDF_EXPORT int FPDF_CALLCONV FPDFPage_GetRotation(FPDF_PAGE page) {
    return call("FPDFPage_GetRotation", raw(page));
}

FPDF_EXPORT float FPDF_CALLCONV FPDF_GetPageWidthF(FPDF_PAGE page) {
    return static_cast<float>(pdfium_call_float("FPDF_GetPageWidthF", raw(page)));
}

FPDF_EXPORT float FPDF_CALLCONV FPDF_GetPageHeightF(FPDF_PAGE page) {
    return static_cast<float>(pdfium_call_float("FPDF_GetPageHeightF", raw(page)));
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDFPage_GetCropBox(FPDF_PAGE page, float *left, float *bottom,
                                                        float *right, float *top) {
    return pdfium_box("FPDFPage_GetCropBox", raw(page), left, bottom, right, top);
}

FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDFPage_GetMediaBox(FPDF_PAGE page, float *left, float *bottom,
                                                         float *right, float *top) {
    return pdfium_box("FPDFPage_GetMediaBox", raw(page), left, bottom, right, top);
}

FPDF_EXPORT void FPDF_CALLCONV FPDFPage_SetMediaBox(FPDF_PAGE page, float left, float bottom,
                                                    float right, float top) {
    pdfium_set_box("FPDFPage_SetMediaBox", raw(page), left, bottom, right, top);
}

FPDF_EXPORT void FPDF_CALLCONV FPDFPage_SetCropBox(FPDF_PAGE page, float left, float bottom,
                                                   float right, float top) {
    pdfium_set_box("FPDFPage_SetCropBox", raw(page), left, bottom, right, top);
}

// PDFium writes the whole file in JavaScript, which hands it to the writer in one block.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV FPDF_SaveAsCopy(FPDF_DOCUMENT document, FPDF_FILEWRITE *writer,
                                                    FPDF_DWORD flags) {
    const int size = pdfium_save(raw(document), static_cast<int>(flags));
    if (size < 0)
        return 0;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    pdfium_take_saved(bytes.data());
    return writer->WriteBlock(writer, bytes.data(), static_cast<unsigned long>(bytes.size()));
}

} // extern "C"
