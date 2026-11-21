#include "board/board_pages.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

Stroke strokeAt(float x, float y) {
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({x, y, 1.0F});
    stroke.addPoint({x + 10.0F, y, 1.0F});
    return stroke;
}

std::vector<std::string> itemIds(const ColumnSpec &column) {
    std::vector<std::string> ids;
    for (const ColumnItem &item : column.items)
        ids.push_back(item.id);
    return ids;
}

// Sends everything `from` has that `to` lacks.
void sync(const Board &from, Board &to) {
    ASSERT_TRUE(to.apply(from.difference(to.stateVector())));
}

class BoardPagesTest : public ::testing::Test {
  protected:
    void SetUp() override { pages().start(paperA4, "Notes"); }
    BoardPages &pages() { return board_.pages(); }
    [[nodiscard]] const ColumnSpec &column(std::size_t i = 0) { return pages().columns().at(i); }
    [[nodiscard]] std::string firstPage() { return column().items.at(0).id; }

    Board board_;
};

TEST(BoardPagesStart, MakesAnEmptyBoardAPageBoardWithOnePage) {
    Board board;
    EXPECT_EQ(board.pages().kind(), BoardKind::Infinite);
    board.pages().start(paperLetter, "Paper");
    EXPECT_EQ(board.pages().kind(), BoardKind::Pages);
    ASSERT_EQ(board.pages().columns().size(), 1U);
    const ColumnSpec &column = board.pages().columns()[0];
    EXPECT_EQ(column.title, "Paper");
    EXPECT_FLOAT_EQ(column.paper.width, paperLetter.width);
    ASSERT_EQ(column.items.size(), 1U);
    EXPECT_EQ(column.items[0].kind, ColumnItemKind::Page);
    EXPECT_FALSE(board.canUndo()); // a board is created as one kind
}

TEST(BoardPagesStart, RefusesBoardsWithContentAndBadPaper) {
    Board drawn;
    drawn.addStroke(strokeAt(0.0F, 0.0F));
    EXPECT_THROW(drawn.pages().start(paperA4, ""), std::logic_error);
    Board twice;
    twice.pages().start(paperA4, "");
    EXPECT_THROW(twice.pages().start(paperA4, ""), std::logic_error);
    Board bad;
    EXPECT_THROW(bad.pages().start({-1.0F, 10.0F}, ""), std::invalid_argument);
}

TEST_F(BoardPagesTest, AddsColumnsBeforeOthersOrLast) {
    const std::string last = pages().addColumn({}, paperLetter, "Last");
    const std::string middle = pages().addColumn(last, paperA5, "Middle");
    ASSERT_EQ(pages().columns().size(), 3U);
    EXPECT_EQ(column(1).id, middle);
    EXPECT_EQ(column(2).id, last);
    EXPECT_EQ(column(2).items.size(), 1U); // every new column starts with a page
    pages().setColumnTitle(middle, "Renamed");
    pages().setColumnPaper(middle, landscape(paperA5));
    EXPECT_EQ(column(1).title, "Renamed");
    EXPECT_FLOAT_EQ(column(1).paper.width, paperA5.height);
    EXPECT_THROW(pages().setColumnPaper(middle, {0.0F, 1.0F}), std::invalid_argument);
    pages().removeColumn(middle);
    EXPECT_EQ(pages().columns().size(), 2U);
}

TEST_F(BoardPagesTest, InsertsPagesAndSubheadingsWhereAsked) {
    const std::string first = firstPage();
    const std::string end = pages().insertItem(column().id, {}, ColumnItemKind::Page);
    const std::string heading =
        pages().insertItem(column().id, first, ColumnItemKind::Subheading, "Model");
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{heading, first, end}));
    EXPECT_EQ(column().items[0].text, "Model");
    pages().setSubheadingText(column().id, heading, "The model");
    EXPECT_EQ(column().items[0].text, "The model");
    pages().removeItem(column().id, first);
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{heading, end}));
    EXPECT_TRUE(pages().insertItem("no such column", {}, ColumnItemKind::Page).empty());
}

TEST_F(BoardPagesTest, KeepsStrokesPerPageAndErasesThem) {
    const std::string page = firstPage();
    const std::string other = pages().insertItem(column().id, {}, ColumnItemKind::Page);
    pages().addStroke(page, strokeAt(10.0F, 10.0F));
    pages().addStroke(page, strokeAt(10.0F, 300.0F));
    pages().addStroke(other, strokeAt(10.0F, 10.0F));
    pages().addStroke("no such page", strokeAt(0.0F, 0.0F));
    EXPECT_EQ(pages().strokes(page).size(), 2U);
    EXPECT_EQ(pages().strokes(other).size(), 1U);
    EXPECT_TRUE(pages().strokes("no such page").empty());
    EXPECT_EQ(pages().eraseNear(page, {12.0F, 300.0F}, 2.0F), 1U);
    ASSERT_EQ(pages().strokes(page).size(), 1U);
    EXPECT_FLOAT_EQ(pages().strokes(page)[0].points()[0].y, 10.0F);
    EXPECT_THROW((void)pages().eraseNear(page, {0.0F, 0.0F}, -1.0F), std::invalid_argument);
}

TEST_F(BoardPagesTest, UndoesEditsOneStepAtATime) {
    const std::string page = firstPage();
    pages().addStroke(page, strokeAt(10.0F, 10.0F));
    pages().addColumn({}, paperA4, "Second");
    pages().removeItem(column().id, page);
    EXPECT_TRUE(column().items.empty());
    ASSERT_TRUE(board_.undo()); // the page comes back with its stroke
    EXPECT_EQ(firstPage(), page);
    EXPECT_EQ(pages().strokes(page).size(), 1U);
    ASSERT_TRUE(board_.undo());
    EXPECT_EQ(pages().columns().size(), 1U);
    ASSERT_TRUE(board_.undo());
    EXPECT_TRUE(pages().strokes(page).empty());
    EXPECT_FALSE(board_.undo());
}

TEST_F(BoardPagesTest, MergesConcurrentEditsTheSameWayOnBothSides) {
    Board peer;
    sync(board_, peer);
    const std::string columnId = column().id;
    const std::string page = firstPage();
    EXPECT_EQ(peer.pages().columns().at(0).id, columnId);
    pages().insertItem(columnId, {}, ColumnItemKind::Page);
    peer.pages().insertItem(columnId, {}, ColumnItemKind::Subheading, "Peer");
    peer.pages().addStroke(page, strokeAt(5.0F, 5.0F));
    EXPECT_TRUE(pages().strokes(page).empty()); // cached before the peer's stroke arrives
    sync(peer, board_);
    sync(board_, peer);
    EXPECT_EQ(itemIds(column()), itemIds(peer.pages().columns().at(0)));
    EXPECT_EQ(column().items.size(), 3U);
    EXPECT_EQ(pages().strokes(page).size(), 1U);
}

TEST_F(BoardPagesTest, SavesAndLoadsWithEverythingInPlace) {
    const std::string page = firstPage();
    pages().insertItem(column().id, page, ColumnItemKind::Subheading, "Intro");
    pages().addStroke(page, strokeAt(1.0F, 2.0F));
    Board loaded;
    ASSERT_TRUE(loaded.apply(board_.encodeState()));
    EXPECT_EQ(loaded.pages().kind(), BoardKind::Pages);
    EXPECT_EQ(itemIds(loaded.pages().columns().at(0)), itemIds(column()));
    EXPECT_EQ(loaded.pages().columns()[0].title, "Notes");
    EXPECT_EQ(loaded.pages().strokes(page).size(), 1U);
}

TEST_F(BoardPagesTest, TellsTheBoardsListenerAboutPageChanges) {
    int changes = 0;
    board_.setChangeListener([&] { ++changes; });
    pages().addStroke(firstPage(), strokeAt(0.0F, 0.0F));
    pages().addColumn({}, paperA4, "");
    EXPECT_GE(changes, 2);
}

} // namespace
} // namespace peerchalk
