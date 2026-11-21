#pragma once

#include "board/stroke_codec.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace peerchalk {

// A page of another PDF to put under a page.
struct PdfUnderlay {
    std::shared_ptr<const Bytes> pdf;
    int page{0};
};

// One page of a combined PDF: a page of another PDF with the overlay page over it, or the
// overlay page alone at an exact size in points (PDF writers may round page sizes).
struct PdfComposedPage {
    std::optional<PdfUnderlay> under;
    float widthPoints{0.0F};
    float heightPoints{0.0F};
};

// The PDF overlay, one page for each of its pages. A page with a page of another PDF under
// it keeps that page as it is (text stays text) and draws the overlay page over it the way
// the page is shown, rotation included, top left corners together. Throws
// std::invalid_argument when a PDF cannot be read or the counts differ, and
// std::runtime_error when PDFium fails.
[[nodiscard]] Bytes composePdf(const Bytes &overlay, const std::vector<PdfComposedPage> &pages);

} // namespace peerchalk
