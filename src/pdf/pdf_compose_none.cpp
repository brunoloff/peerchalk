// composePdf for builds without PDFium: only overlays with nothing under them, at the sizes
// their writer gave them.
#include "pdf/pdf_compose.hpp"

#include <algorithm>
#include <stdexcept>

namespace peerchalk {

Bytes composePdf(const Bytes &overlay, const std::vector<PdfComposedPage> &pages) {
    if (std::ranges::any_of(pages,
                            [](const PdfComposedPage &page) { return page.under.has_value(); }))
        throw std::invalid_argument("This version of PeerChalk cannot export PDF pages");
    return overlay;
}

} // namespace peerchalk
