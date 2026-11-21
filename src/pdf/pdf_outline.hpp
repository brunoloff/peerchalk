#pragma once

#include "board/stroke_codec.hpp"

#include <string>
#include <vector>

namespace peerchalk {

// A bookmark: a heading (level 1) or a subheading under the heading before it (level 2),
// leading to a page (from 0).
struct PdfOutlineEntry {
    std::string title; // UTF-8
    int level{1};
    int page{0};

    bool operator==(const PdfOutlineEntry &) const = default;
};

// The PDF with these bookmarks, added as an incremental update, so the rest of the file is
// left byte for byte. It reads only what it needs: a classic cross-reference table, the
// catalog and the page tree, as PDFium and Qt write them. Returns the PDF unchanged when
// there are no entries; throws std::invalid_argument for a PDF it cannot follow or a page
// that does not exist.
[[nodiscard]] Bytes addPdfOutline(const Bytes &pdf, const std::vector<PdfOutlineEntry> &entries);

} // namespace peerchalk
