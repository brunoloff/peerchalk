#include "pdf/pdf_document.hpp"

#include "board/rich_text.hpp"
#include "pdf/pdf_engine.hpp"

#include <fpdf_doc.h>
#include <fpdfview.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr float unitsPerPoint = 96.0F / 72.0F;
constexpr int maxRenderSide = 16384;

using pdf_engine::initialise;

std::mutex &engineLock() { return pdf_engine::lock(); }

struct PageCloser {
    void operator()(fpdf_page_t__ *page) const { FPDF_ClosePage(page); }
};
using Page = std::unique_ptr<fpdf_page_t__, PageCloser>;

struct BitmapCloser {
    void operator()(fpdf_bitmap_t__ *bitmap) const { FPDFBitmap_Destroy(bitmap); }
};
using Bitmap = std::unique_ptr<fpdf_bitmap_t__, BitmapCloser>;

} // namespace

namespace pdf_engine {

std::mutex &lock() {
    static std::mutex engine;
    return engine;
}

void initialise() {
    static std::once_flag once;
    std::call_once(once, [] {
        FPDF_LIBRARY_CONFIG config{};
        config.version = 2;
        FPDF_InitLibraryWithConfig(&config);
    });
}

} // namespace pdf_engine

struct PdfDocument::Native {
    std::shared_ptr<const Bytes> data; // PDFium reads from it for as long as the document is open
    FPDF_DOCUMENT document{nullptr};
    int pages{0};

    void checkPage(int index) const {
        if (index < 0 || index >= pages)
            throw std::out_of_range("The PDF has no such page");
    }
};

PdfDocument::PdfDocument(std::unique_ptr<Native> native) : native_(std::move(native)) {}

PdfDocument::~PdfDocument() {
    const std::lock_guard lock(engineLock());
    FPDF_CloseDocument(native_->document);
}

bool PdfDocument::available() { return true; }

PdfDocument::Opened PdfDocument::open(std::shared_ptr<const Bytes> data) {
    if (!data || data->empty())
        return {nullptr, PdfError::Unreadable};
    const std::lock_guard lock(engineLock());
    initialise();
    auto native = std::make_unique<Native>();
    native->data = std::move(data);
    native->document = FPDF_LoadMemDocument64(native->data->data(), native->data->size(), nullptr);
    if (native->document == nullptr) {
        const unsigned long error = FPDF_GetLastError();
        return {nullptr, error == FPDF_ERR_PASSWORD ? PdfError::Password : PdfError::Unreadable};
    }
    native->pages = FPDF_GetPageCount(native->document);
    return {std::unique_ptr<PdfDocument>(new PdfDocument(std::move(native))), std::nullopt};
}

int PdfDocument::pageCount() const { return native_->pages; }

PaperSize PdfDocument::pageSize(int index) const {
    native_->checkPage(index);
    const std::lock_guard lock(engineLock());
    FS_SIZEF size{};
    if (!FPDF_GetPageSizeByIndexF(native_->document, index, &size) || size.width <= 0.0F ||
        size.height <= 0.0F)
        throw std::out_of_range("The PDF page has no size");
    return {size.width * unitsPerPoint, size.height * unitsPerPoint};
}

std::string PdfDocument::title() const {
    const std::lock_guard lock(engineLock());
    const unsigned long bytes = FPDF_GetMetaText(native_->document, "Title", nullptr, 0);
    if (bytes <= 2) // just the terminator
        return {};
    std::u16string text((bytes / 2) - 1, u'\0');
    std::vector<unsigned short> buffer(bytes / 2);
    FPDF_GetMetaText(native_->document, "Title", buffer.data(), bytes);
    for (std::size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char16_t>(buffer[i]);
    return toUtf8(text);
}

namespace {

std::string bookmarkTitle(FPDF_BOOKMARK bookmark) {
    const unsigned long bytes = FPDFBookmark_GetTitle(bookmark, nullptr, 0);
    std::vector<unsigned short> buffer(bytes / 2 + 1);
    FPDFBookmark_GetTitle(bookmark, buffer.data(), bytes);
    std::u16string text;
    for (std::size_t i = 0; i + 1 < bytes / 2 + 1 && buffer[i] != 0; ++i)
        text += static_cast<char16_t>(buffer[i]);
    return toUtf8(text);
}

void readBookmarks(FPDF_DOCUMENT document, FPDF_BOOKMARK parent, int level,
                   std::vector<PdfOutlineEntry> &out) {
    for (FPDF_BOOKMARK item = FPDFBookmark_GetFirstChild(document, parent); item != nullptr;
         item = FPDFBookmark_GetNextSibling(document, item)) {
        const FPDF_DEST dest = FPDFBookmark_GetDest(document, item);
        const int page = dest != nullptr ? FPDFDest_GetDestPageIndex(document, dest) : -1;
        if (page >= 0)
            out.push_back({bookmarkTitle(item), level, page});
        if (level < 2)
            readBookmarks(document, item, level + 1, out);
    }
}

} // namespace

std::vector<PdfOutlineEntry> PdfDocument::outline() const {
    const std::lock_guard lock(engineLock());
    std::vector<PdfOutlineEntry> entries;
    readBookmarks(native_->document, nullptr, 1, entries);
    return entries;
}

PdfImage PdfDocument::render(int index, int width, int height) const {
    return renderTile(index, width, height, 0, 0, width, height);
}

PdfImage PdfDocument::renderTile(int index, int pageWidth, int pageHeight, int x, int y, int width,
                                 int height) const {
    native_->checkPage(index);
    constexpr int maxPageSide = 65536;
    if (width < 1 || height < 1 || width > maxRenderSide || height > maxRenderSide ||
        pageWidth < 1 || pageHeight < 1 || pageWidth > maxPageSide || pageHeight > maxPageSide)
        throw std::invalid_argument("PDF pages render at 1 to 16384 pixels a side");
    const std::lock_guard lock(engineLock());
    const Page page(FPDF_LoadPage(native_->document, index));
    const Bitmap bitmap(FPDFBitmap_Create(width, height, 0));
    if (!page || !bitmap)
        throw std::runtime_error("PDFium could not prepare the page");
    FPDFBitmap_FillRect(bitmap.get(), 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap.get(), page.get(), -x, -y, pageWidth, pageHeight, 0, FPDF_ANNOT);
    PdfImage image{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
    const auto *source = static_cast<const std::uint8_t *>(FPDFBitmap_GetBuffer(bitmap.get()));
    const int stride = FPDFBitmap_GetStride(bitmap.get());
    for (int row = 0; row < height; ++row)
        std::memcpy(image.pixels.data() + std::size_t(row) * width * 4,
                    source + std::size_t(row) * stride, std::size_t(width) * 4);
    return image;
}

} // namespace peerchalk
