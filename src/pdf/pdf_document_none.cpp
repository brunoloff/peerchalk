// PdfDocument for builds without a PDF engine: every PDF is refused as unsupported.
#include "pdf/pdf_document.hpp"

#include <stdexcept>

namespace peerchalk {

struct PdfDocument::Native {};

PdfDocument::PdfDocument(std::unique_ptr<Native> native) : native_(std::move(native)) {}
PdfDocument::~PdfDocument() = default;

bool PdfDocument::available() { return false; }

PdfDocument::Opened PdfDocument::open(std::shared_ptr<const Bytes>) {
    return {nullptr, PdfError::Unsupported};
}

int PdfDocument::pageCount() const { return 0; }

PaperSize PdfDocument::pageSize(int) const { throw std::out_of_range("No PDF engine"); }

std::string PdfDocument::title() const { return {}; }

std::vector<PdfOutlineEntry> PdfDocument::outline() const { return {}; }

PdfImage PdfDocument::render(int, int, int) const { throw std::out_of_range("No PDF engine"); }

PdfImage PdfDocument::renderTile(int, int, int, int, int, int, int) const {
    throw std::out_of_range("No PDF engine");
}

} // namespace peerchalk
