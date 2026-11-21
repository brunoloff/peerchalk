#include "pdf/pdf_compose.hpp"

#include "pdf/pdf_engine.hpp"

#include <fpdf_edit.h>
#include <fpdf_ppo.h>
#include <fpdf_save.h>
#include <fpdf_transformpage.h>
#include <fpdfview.h>

#include <map>
#include <stdexcept>

namespace peerchalk {
namespace {

struct DocumentCloser {
    void operator()(fpdf_document_t__ *document) const { FPDF_CloseDocument(document); }
};
using Document = std::unique_ptr<fpdf_document_t__, DocumentCloser>;

struct PageCloser {
    void operator()(fpdf_page_t__ *page) const { FPDF_ClosePage(page); }
};
using Page = std::unique_ptr<fpdf_page_t__, PageCloser>;

Document load(const Bytes &pdf) {
    Document document(FPDF_LoadMemDocument64(pdf.data(), pdf.size(), nullptr));
    if (!document)
        throw std::invalid_argument("A PDF to combine could not be read");
    return document;
}

struct Writer : FPDF_FILEWRITE {
    Bytes bytes;

    Writer() : FPDF_FILEWRITE{} {
        version = 1;
        WriteBlock = [](FPDF_FILEWRITE *self, const void *data, unsigned long size) {
            auto &out = static_cast<Writer *>(self)->bytes;
            const auto *begin = static_cast<const std::byte *>(data);
            out.insert(out.end(), begin, begin + size);
            return 1;
        };
    }
};

// Where the page is shown, in its own coordinates. PDFium's box getters take left, bottom,
// right, top.
FS_RECTF shownBox(FPDF_PAGE page) {
    FS_RECTF box{};
    if (!FPDFPage_GetCropBox(page, &box.left, &box.bottom, &box.right, &box.top) &&
        !FPDFPage_GetMediaBox(page, &box.left, &box.bottom, &box.right, &box.top))
        box = {0.0F, FPDF_GetPageHeightF(page), FPDF_GetPageWidthF(page), 0.0F};
    return box;
}

// Maps the overlay page, upright and overlayHeight tall, onto the page's own coordinates,
// given the box shown and the page's rotation (quarter turns clockwise), so that the top left
// corners meet.
FS_MATRIX overlayMatrix(const FS_RECTF &box, int rotation, float overlayHeight) {
    FS_MATRIX m{1, 0, 0, 1, box.left, box.bottom};
    float shownHeight = box.top - box.bottom;
    switch (rotation) {
    case 1:
        m = {0, 1, -1, 0, box.right, box.bottom};
        shownHeight = box.right - box.left;
        break;
    case 2:
        m = {-1, 0, 0, -1, box.right, box.top};
        break;
    case 3:
        m = {0, -1, 1, 0, box.left, box.top};
        shownHeight = box.right - box.left;
        break;
    default:
        break;
    }
    const float shift = shownHeight - overlayHeight; // moves the overlay's top to the page's
    m.e += m.c * shift;
    m.f += m.d * shift;
    return m;
}

// Puts page `index` of overlay over page `index` of result, which came from another PDF.
void lay(FPDF_DOCUMENT result, FPDF_DOCUMENT overlay, int index) {
    FS_SIZEF overlaySize{};
    FPDF_GetPageSizeByIndexF(overlay, index, &overlaySize);
    const Page page(FPDF_LoadPage(result, index));
    FPDF_XOBJECT form = FPDF_NewXObjectFromPage(result, overlay, index);
    FPDF_PAGEOBJECT object = form != nullptr ? FPDF_NewFormObjectFromXObject(form) : nullptr;
    if (!page || object == nullptr) {
        if (form != nullptr)
            FPDF_CloseXObject(form);
        throw std::runtime_error("PDFium could not combine a page");
    }
    const FS_MATRIX m =
        overlayMatrix(shownBox(page.get()), FPDFPage_GetRotation(page.get()), overlaySize.height);
    FPDFPageObj_Transform(object, m.a, m.b, m.c, m.d, m.e, m.f);
    FPDFPage_InsertObject(page.get(), object);
    FPDF_CloseXObject(form);
    if (!FPDFPage_GenerateContent(page.get()))
        throw std::runtime_error("PDFium could not write a combined page");
}

// Gives an overlay page its exact size, keeping its top left corner where it is.
void resize(FPDF_DOCUMENT result, int index, float width, float height) {
    const Page page(FPDF_LoadPage(result, index));
    if (!page)
        throw std::runtime_error("PDFium could not size a page");
    const FS_RECTF box = shownBox(page.get());
    FPDFPage_SetMediaBox(page.get(), box.left, box.top - height, box.left + width, box.top);
    FPDFPage_SetCropBox(page.get(), box.left, box.top - height, box.left + width, box.top);
}

} // namespace

Bytes composePdf(const Bytes &overlay, const std::vector<PdfComposedPage> &pages) {
    const std::lock_guard lock(pdf_engine::lock());
    pdf_engine::initialise();
    const Document top = load(overlay);
    if (FPDF_GetPageCount(top.get()) != static_cast<int>(pages.size()))
        throw std::invalid_argument("Each page of the overlay needs an entry");
    const Document result(FPDF_CreateNewDocument());
    std::map<const Bytes *, Document> sources;
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const PdfComposedPage &page = pages[std::size_t(i)];
        FPDF_DOCUMENT source = top.get();
        int sourcePage = i;
        if (page.under) {
            auto &loaded = sources[page.under->pdf.get()];
            if (!loaded)
                loaded = load(*page.under->pdf);
            source = loaded.get();
            sourcePage = page.under->page;
        }
        if (!FPDF_ImportPagesByIndex(result.get(), source, &sourcePage, 1, i))
            throw std::invalid_argument("A page to combine does not exist");
        if (page.under)
            lay(result.get(), top.get(), i);
        else if (page.widthPoints > 0.0F && page.heightPoints > 0.0F)
            resize(result.get(), i, page.widthPoints, page.heightPoints);
    }
    Writer writer;
    if (!FPDF_SaveAsCopy(result.get(), &writer, FPDF_NO_INCREMENTAL))
        throw std::runtime_error("PDFium could not save the PDF");
    return std::move(writer.bytes);
}

} // namespace peerchalk
