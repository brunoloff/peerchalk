// Pages that show the pages of a PDF (see BoardPages::addPdfColumn).
#include "board/board_pages_native.hpp"

#include <cmath>
#include <map>

namespace peerchalk {
namespace {

using namespace page_yrs;

// The size most pages have, to the nearest half unit; the first such page's on a tie.
PaperSize mostCommon(const std::vector<PaperSize> &sizes) {
    std::map<std::pair<long, long>, int> counts;
    const auto key = [](PaperSize size) {
        return std::pair{std::lround(size.width * 2.0F), std::lround(size.height * 2.0F)};
    };
    for (const PaperSize size : sizes)
        ++counts[key(size)];
    PaperSize best = sizes.front();
    for (const PaperSize size : sizes) {
        if (counts[key(size)] > counts[key(best)])
            best = size;
    }
    return best;
}

} // namespace

std::string BoardPages::addPdfColumn(const std::string &before, const std::string &title,
                                     const std::string &blob,
                                     const std::vector<PaperSize> &pageSizes) {
    if (blob.empty() || pageSizes.empty())
        throw std::invalid_argument("A PDF column needs its PDF and at least one page");
    ColumnSpec spec{newId(), mostCommon(pageSizes), {}, title, PageBackground::Blank};
    for (std::size_t i = 0; i < pageSizes.size(); ++i) {
        checkPaper(pageSizes[i]);
        spec.items.push_back({newId(),
                              ColumnItemKind::Page,
                              pageSizes[i],
                              {},
                              PdfSource{blob, static_cast<int>(i)}});
    }
    yundo_manager_stop(native_.undo.get());
    {
        const BoardTransaction txn = native_.write(true);
        const std::uint32_t index =
            indexOf(native_.columns, txn.get(), before).value_or(yarray_len(native_.columns));
        insertColumnSpec(native_, txn.get(), index, spec);
        for (const ColumnItem &page : spec.items)
            createStrokeArray(native_, txn.get(), page.id);
    }
    yundo_manager_stop(native_.undo.get());
    return spec.id;
}

std::set<std::string> BoardPages::pdfBlobs() const {
    std::set<std::string> blobs;
    for (const ColumnSpec &column : columns()) {
        for (const ColumnItem &item : column.items) {
            if (item.pdf)
                blobs.insert(item.pdf->blob);
        }
    }
    return blobs;
}

} // namespace peerchalk
