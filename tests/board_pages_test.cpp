#include "board/board_pages.hpp"

#include <gtest/gtest.h>

#include <set>
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
    board.pages().start(paperLetter, "Paper", PageBackground::Ruled);
    EXPECT_EQ(board.pages().kind(), BoardKind::Pages);
    ASSERT_EQ(board.pages().columns().size(), 1U);
    const ColumnSpec &column = board.pages().columns()[0];
    EXPECT_EQ(column.title, "Paper");
    EXPECT_EQ(column.background, PageBackground::Ruled);
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
    EXPECT_EQ(column(1).background, PageBackground::Blank);
    pages().setColumnBackground(middle, PageBackground::Squared);
    EXPECT_EQ(column(1).background, PageBackground::Squared);
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

TEST_F(BoardPagesTest, MovesPagesWithinAndAcrossColumnsKeepingTheirStrokes) {
    const std::string columnId = column().id;
    const std::string first = firstPage();
    const std::string second = pages().insertItem(columnId, {}, ColumnItemKind::Page);
    pages().addStroke(second, strokeAt(3.0F, 3.0F));
    pages().moveItem(columnId, second, columnId, first);
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{second, first}));
    const std::string other = pages().addColumn({}, paperLetter, "Other");
    pages().moveItem(columnId, second, other, {});
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{first}));
    EXPECT_EQ(column(1).items.back().id, second);
    EXPECT_EQ(pages().strokes(second).size(), 1U);
    ASSERT_TRUE(board_.undo()); // one step back to the first column
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{second, first}));
}

TEST_F(BoardPagesTest, MovesColumnsWithTheirItems) {
    const std::string first = column().id;
    const std::string page = firstPage();
    pages().insertItem(first, {}, ColumnItemKind::Subheading, "End");
    const std::string second = pages().addColumn({}, paperLetter, "Second");
    pages().moveColumn(first, {});
    EXPECT_EQ(column(0).id, second);
    EXPECT_EQ(column(1).id, first);
    EXPECT_EQ(column(1).title, "Notes");
    ASSERT_EQ(column(1).items.size(), 2U);
    EXPECT_EQ(column(1).items[0].id, page);
    EXPECT_EQ(column(1).items[1].text, "End");
    pages().moveColumn(first, second);
    EXPECT_EQ(column(0).id, first);
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

TEST_F(BoardPagesTest, CountsChangesPerPageAndToTheColumns) {
    const std::string page = firstPage();
    const std::string other = pages().insertItem(column().id, {}, ColumnItemKind::Page);
    const std::uint64_t columns = pages().columnsVersion();
    const std::uint64_t otherStrokes = pages().strokesVersion(other);
    EXPECT_EQ(pages().strokesVersion("no such page"), 0U);
    const std::uint64_t before = pages().strokesVersion(page);
    pages().addStroke(page, strokeAt(1.0F, 1.0F));
    EXPECT_GT(pages().strokesVersion(page), before);
    EXPECT_EQ(pages().strokesVersion(other), otherStrokes);
    EXPECT_EQ(pages().columnsVersion(), columns);
    pages().setColumnTitle(column().id, "Changed");
    EXPECT_GT(pages().columnsVersion(), columns);
}

TEST_F(BoardPagesTest, KeepsTextBoxesWithTheirPageAndRemovesThemWithIt) {
    const std::string page = firstPage();
    TextBox box;
    box.id = "box";
    box.page = page;
    box.x = 10.0F;
    box.y = 20.0F;
    box.body = plainRichText(u"Lemma");
    board_.addTextBox(box);
    ASSERT_NE(board_.textBox("box"), nullptr);
    EXPECT_EQ(board_.textBox("box")->page, page);
    EXPECT_FALSE(board_.contentBounds()); // page coordinates are not board coordinates
    EXPECT_EQ(board_.eraseNear({12.0F, 22.0F}, 4.0F), 0U);
    pages().moveItem(column().id, page, pages().addColumn({}, paperA4, ""), {});
    EXPECT_NE(board_.textBox("box"), nullptr); // moving keeps it
    pages().removeItem(column(1).id, page);
    EXPECT_EQ(board_.textBox("box"), nullptr);
    ASSERT_TRUE(board_.undo());
    EXPECT_NE(board_.textBox("box"), nullptr);
}

TEST_F(BoardPagesTest, ChangesAColumnsPaperForNewPagesOrForAll) {
    const std::string columnId = column().id;
    const std::string old = firstPage();
    pages().setColumnPaper(columnId, paperLetter);
    const std::string added = pages().insertItem(columnId, {}, ColumnItemKind::Page);
    ASSERT_TRUE(column().items[0].size); // the existing page keeps A4
    EXPECT_FLOAT_EQ(column().items[0].size->height, paperA4.height);
    EXPECT_FALSE(column().items[1].size); // a new page takes the column's Letter
    EXPECT_FLOAT_EQ(column().paper.width, paperLetter.width);
    pages().setColumnPaper(columnId, paperA5, true);
    EXPECT_FALSE(column().items[0].size);
    EXPECT_FALSE(column().items[1].size);
    EXPECT_FLOAT_EQ(column().paper.height, paperA5.height);
    EXPECT_EQ(itemIds(column()), (std::vector<std::string>{old, added}));
}

TEST_F(BoardPagesTest, GivesAPageItsOwnSizeAndTakesItBack) {
    const std::string columnId = column().id;
    const std::string page = firstPage();
    const std::string heading =
        pages().insertItem(columnId, {}, ColumnItemKind::Subheading, "Part");
    pages().setPageSize(columnId, page, landscape(paperA4));
    ASSERT_TRUE(column().items[0].size);
    EXPECT_FLOAT_EQ(column().items[0].size->width, paperA4.height);
    pages().setPageSize(columnId, heading, paperA4); // only pages have sizes
    EXPECT_FALSE(column().items[1].size);
    pages().setPageSize(columnId, page, std::nullopt);
    EXPECT_FALSE(column().items[0].size);
    EXPECT_THROW(pages().setPageSize(columnId, page, PaperSize{1.0F, -1.0F}),
                 std::invalid_argument);
    ASSERT_TRUE(board_.undo());
    EXPECT_TRUE(column().items[0].size);
}

TEST_F(BoardPagesTest, AddsAColumnOfPdfPagesThatKeepTheirSizes) {
    const std::string first = column().id;
    const std::vector<PaperSize> sizes = {paperLetter, landscape(paperLetter), paperLetter};
    const std::string pdf = pages().addPdfColumn(first, "Paper", "ab12", sizes);
    ASSERT_EQ(column(0).id, pdf);
    EXPECT_EQ(column(0).title, "Paper");
    EXPECT_FLOAT_EQ(column(0).paper.width, paperLetter.width); // the most common size
    ASSERT_EQ(column(0).items.size(), 3U);
    const ColumnItem turned = column(0).items[1];
    ASSERT_TRUE(turned.pdf);
    EXPECT_EQ(turned.pdf->blob, "ab12");
    EXPECT_EQ(turned.pdf->page, 1);
    EXPECT_FLOAT_EQ(turned.size->width, paperLetter.height);
    EXPECT_EQ(pages().pdfBlobs(), (std::set<std::string>{"ab12"}));
    pages().setColumnPaper(pdf, paperA4, true); // PDF pages are never resized
    pages().setPageSize(pdf, turned.id, paperA4);
    EXPECT_FLOAT_EQ(column(0).items[1].size->width, paperLetter.height);
    pages().addStroke(turned.id, strokeAt(1.0F, 1.0F));
    EXPECT_EQ(pages().strokes(turned.id).size(), 1U);
    Board loaded;
    ASSERT_TRUE(loaded.apply(board_.encodeState()));
    EXPECT_EQ(loaded.pages().columns()[0].items[2].pdf, (PdfSource{"ab12", 2}));
    ASSERT_TRUE(board_.undo()); // the stroke
    ASSERT_TRUE(board_.undo()); // the paper change
    ASSERT_TRUE(board_.undo()); // the PDF column
    EXPECT_EQ(pages().columns().size(), 1U);
    EXPECT_THROW(pages().addPdfColumn({}, "", "ab12", {}), std::invalid_argument);
    EXPECT_THROW(pages().addPdfColumn({}, "", "", {paperA4}), std::invalid_argument);
}

} // namespace
} // namespace peerchalk
