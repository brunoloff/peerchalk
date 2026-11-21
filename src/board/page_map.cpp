#include "board/page_map.hpp"

#include "board/board_pages.hpp"

namespace peerchalk {

bool PageMap::refresh(const BoardPages &pages) {
    if (source_ == &pages && version_ == pages.columnsVersion())
        return false;
    source_ = &pages;
    version_ = pages.columnsVersion();
    layout_ = pages.kind() == BoardKind::Pages ? layoutPages(pages.columns()) : PageLayout{};
    byId_.clear();
    for (std::size_t c = 0; c < layout_.columns.size(); ++c) {
        const std::vector<PlacedItem> &items = layout_.columns[c].items;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (items[i].kind == ColumnItemKind::Page)
                byId_.emplace(items[i].id, ItemRef{c, i});
        }
    }
    return true;
}

void PageMap::clear() {
    source_ = nullptr;
    layout_ = {};
    byId_.clear();
}

PlacedPage PageMap::placed(ItemRef ref) const {
    const PlacedColumn &column = layout_.columns[ref.column];
    const PlacedItem &item = column.items[ref.item];
    return {item.id, column.id, item.bounds};
}

std::optional<PlacedPage> PageMap::page(const std::string &id) const {
    const auto found = byId_.find(id);
    return found != byId_.end() ? std::optional(placed(found->second)) : std::nullopt;
}

std::optional<PlacedPage> PageMap::pageAt(Point point) const {
    const std::optional<ItemRef> ref = peerchalk::pageAt(layout_, point);
    return ref ? std::optional(placed(*ref)) : std::nullopt;
}

std::vector<PlacedPage> PageMap::pagesIn(const Bounds &area) const {
    std::vector<PlacedPage> pages;
    for (const ItemRef ref : itemsIn(layout_, area)) {
        if (layout_.columns[ref.column].items[ref.item].kind == ColumnItemKind::Page)
            pages.push_back(placed(ref));
    }
    return pages;
}

std::optional<Bounds> PageMap::bounds() const {
    if (layout_.columns.empty())
        return std::nullopt;
    Bounds all = layout_.columns.front().bounds;
    for (const PlacedColumn &column : layout_.columns)
        all = unite(all, column.bounds);
    return all;
}

} // namespace peerchalk
