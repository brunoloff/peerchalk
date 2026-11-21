#include "board/board_pages.hpp"
#include "board/page_map.hpp"

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

TEST(PageMap, FindsPagesByIdAndPositionAndFollowsColumnChanges) {
    Board board;
    PageMap map;
    EXPECT_TRUE(map.refresh(board.pages()));
    EXPECT_FALSE(map.bounds()); // an infinite board has no pages
    board.pages().start(paperA4, "One");
    const ColumnSpec &column = board.pages().columns().at(0);
    const std::string columnId = column.id;
    const std::string first = column.items.at(0).id;
    EXPECT_TRUE(map.refresh(board.pages()));
    EXPECT_FALSE(map.refresh(board.pages())); // nothing changed
    const std::optional<PlacedPage> placed = map.page(first);
    ASSERT_TRUE(placed);
    EXPECT_EQ(placed->column, columnId);
    EXPECT_FLOAT_EQ(placed->bounds.width(), paperA4.width);
    const Point inside{placed->bounds.left + 5.0F, placed->bounds.top + 5.0F};
    ASSERT_TRUE(map.pageAt(inside));
    EXPECT_EQ(map.pageAt(inside)->id, first);
    EXPECT_FALSE(map.pageAt({placed->bounds.left - 5.0F, placed->bounds.top}));
    EXPECT_FALSE(map.page("no such page"));
    const std::string second = board.pages().insertItem(columnId, first, ColumnItemKind::Page);
    EXPECT_TRUE(map.refresh(board.pages()));
    EXPECT_LT(map.page(second)->bounds.top, map.page(first)->bounds.top);
    EXPECT_EQ(map.pagesIn(*map.bounds()).size(), 2U);
    map.clear();
    EXPECT_FALSE(map.page(first));
    EXPECT_TRUE(map.refresh(board.pages()));
}

} // namespace
} // namespace peerchalk
