#pragma once

#include "board/page_layout.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace peerchalk {

class BoardPages;

// A page on a page board, where the layout puts it in board coordinates.
struct PlacedPage {
    std::string id;
    std::string column;
    Bounds bounds;
    PageBackground background;
};

// A page board's layout with its pages found by id or by position. refresh() lays the board
// out again only when its columns changed.
class PageMap {
  public:
    // Returns whether the layout changed.
    bool refresh(const BoardPages &pages);
    void clear();

    [[nodiscard]] const PageLayout &layout() const { return layout_; }
    [[nodiscard]] std::optional<PlacedPage> page(const std::string &id) const;
    [[nodiscard]] std::optional<PlacedPage> pageAt(Point point) const;
    // The pages that overlap area, column by column, top to bottom.
    [[nodiscard]] std::vector<PlacedPage> pagesIn(const Bounds &area) const;
    // Everything the layout places; empty without columns.
    [[nodiscard]] std::optional<Bounds> bounds() const;

  private:
    [[nodiscard]] PlacedPage placed(ItemRef ref) const;

    const BoardPages *source_{nullptr};
    std::uint64_t version_{0};
    PageLayout layout_;
    std::unordered_map<std::string, ItemRef> byId_;
};

} // namespace peerchalk
