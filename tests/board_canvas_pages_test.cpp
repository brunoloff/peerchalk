#include "board/board_pages.hpp"
#include "ui/board_canvas.hpp"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <algorithm>

namespace peerchalk {
namespace {

Stroke strokeAt(float x, float y) {
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({x, y, 1.0F});
    stroke.addPoint({x + 10.0F, y, 1.0F});
    return stroke;
}

// The vertices of a page node's mesh: page transform, clip, then the merged mesh.
int meshVertices(const QSGNode *page) {
    const QSGNode *clip = page != nullptr ? page->firstChild() : nullptr;
    const auto *mesh =
        clip != nullptr ? static_cast<const QSGGeometryNode *>(clip->firstChild()) : nullptr;
    return mesh != nullptr ? mesh->geometry()->vertexCount() : 0;
}

class BoardCanvasPagesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        document_.board().pages().start(paperA4, "Notes");
        page_ = document_.board().pages().columns().at(0).items.at(0).id;
        canvas_.setParentItem(window_.contentItem());
        canvas_.setSize(QSizeF(900, 600));
        canvas_.setDocument(&document_);
        window_.resize(900, 600);
        window_.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window_));
        ASSERT_TRUE(canvas_.pageBoard());
        page = canvas_.pageMap().page(page_)->bounds;
    }

    // Where a point of the first page is on screen.
    [[nodiscard]] QPoint screen(float x, float y) const {
        return canvas_.toScreen(QPointF(page.left + x, page.top + y)).toPoint();
    }

    void drag(QPoint from, QPoint to) {
        QTest::mousePress(&window_, Qt::LeftButton, {}, from);
        QTest::mouseMove(&window_, (from + to) / 2);
        QTest::mouseRelease(&window_, Qt::LeftButton, {}, to);
    }

    [[nodiscard]] const std::vector<Stroke> &pageStrokes() {
        return document_.board().pages().strokes(page_);
    }

    QQuickWindow window_;
    BoardDocument document_;
    BoardCanvas canvas_;
    std::string page_;
    Bounds page{}; // the first page on the board
};

TEST_F(BoardCanvasPagesTest, DrawsOnThePageUnderTheStrokeInPageCoordinates) {
    drag(screen(50.0F, 40.0F), screen(120.0F, 60.0F));
    ASSERT_EQ(pageStrokes().size(), 1U);
    EXPECT_TRUE(document_.board().strokes().empty());
    EXPECT_NEAR(pageStrokes()[0].points().front().x, 50.0F, 0.6F); // within a pixel
    EXPECT_NEAR(pageStrokes()[0].points().front().y, 40.0F, 0.6F);
}

TEST_F(BoardCanvasPagesTest, DropsStrokesStartedBetweenPages) {
    drag(screen(50.0F, -60.0F), screen(80.0F, 40.0F)); // starts in the heading band
    EXPECT_TRUE(pageStrokes().empty());
}

TEST_F(BoardCanvasPagesTest, ErasesPageStrokesAndUndoBringsThemBack) {
    document_.board().pages().addStroke(page_, strokeAt(40.0F, 30.0F));
    canvas_.setTool(BoardCanvas::Tool::Eraser);
    drag(screen(45.0F, 30.0F), screen(46.0F, 30.0F));
    EXPECT_TRUE(pageStrokes().empty());
    canvas_.undo();
    EXPECT_EQ(pageStrokes().size(), 1U);
}

TEST_F(BoardCanvasPagesTest, PutsTextBoxesOnPagesAndMovesThemInPageCoordinates) {
    canvas_.setTool(BoardCanvas::Tool::Text);
    QTest::mouseClick(&window_, Qt::LeftButton, {}, screen(60.0F, 50.0F));
    ASSERT_EQ(document_.board().textBoxes().size(), 1U);
    const TextBox box = document_.board().textBoxes()[0];
    EXPECT_EQ(box.page, page_);
    EXPECT_NEAR(box.x, 60.0F, 0.6F);
    const QModelIndex row = canvas_.textBoxes()->index(0, 0);
    const int xRole = canvas_.textBoxes()->roleNames().key("boxX");
    EXPECT_FLOAT_EQ(row.data(xRole).toFloat(), page.left + box.x); // board positions
    canvas_.moveTextBox(QString::fromStdString(box.id), QPointF(page.left, page.top + 90.0));
    EXPECT_FLOAT_EQ(document_.board().textBox(box.id)->y, 90.0F);
    canvas_.finishTextEditing(); // an empty box goes away
    QTest::mouseClick(&window_, Qt::LeftButton, {}, screen(60.0F, -60.0F)); // no page: no box
    EXPECT_TRUE(document_.board().textBoxes().empty());
}

TEST_F(BoardCanvasPagesTest, OpensOnTheTopOfTheFirstColumnAtFullSize) {
    EXPECT_DOUBLE_EQ(canvas_.zoom(), 1.0); // the column fits the width
    const QRectF visible = canvas_.visibleArea();
    EXPECT_NEAR(visible.center().x(), (page.left + page.right) / 2.0, 0.01);
    EXPECT_LT(visible.top(), 0.0); // the heading band is in view
}

TEST_F(BoardCanvasPagesTest, FitsThePagesAndReportsTheirBounds) {
    const QRectF bounds = canvas_.contentBounds();
    EXPECT_FLOAT_EQ(static_cast<float>(bounds.width()), paperA4.width);
    canvas_.fitContent();
    EXPECT_LT(canvas_.zoom(), 1.0);
    EXPECT_TRUE(canvas_.visibleArea().contains(bounds.center()));
}

TEST_F(BoardCanvasPagesTest, EditsColumnsAndItemsUnlessReadOnly) {
    const std::string column = document_.board().pages().columns().at(0).id;
    const QString columnId = QString::fromStdString(column);
    canvas_.addPage(columnId, QString::fromStdString(page_));
    canvas_.addSubheading(columnId, {}, "End");
    canvas_.setColumnPaper(columnId, "Letter", true, true);
    canvas_.setColumnBackground(columnId, "dotted");
    canvas_.setColumnTitle(columnId, "Renamed");
    canvas_.addColumn({});
    const std::vector<ColumnSpec> &columns = document_.board().pages().columns();
    ASSERT_EQ(columns.size(), 2U);
    ASSERT_EQ(columns[0].items.size(), 3U);
    EXPECT_EQ(columns[0].items[1].id, page_); // the new page went before it
    EXPECT_EQ(columns[0].items[2].text, "End");
    EXPECT_EQ(columns[0].title, "Renamed");
    EXPECT_EQ(columns[0].background, PageBackground::Dotted);
    EXPECT_FLOAT_EQ(columns[1].paper.width, paperLetter.height); // copied from its left
    EXPECT_EQ(columns[1].background, PageBackground::Dotted);
    const QVariantMap first = canvas_.pageColumns().at(0).toMap();
    EXPECT_EQ(first["paper"].toString(), "Letter");
    EXPECT_TRUE(first["landscape"].toBool());
    EXPECT_EQ(first["pages"].toInt(), 2);
    canvas_.setColumnPaper(columnId, "B5", false, false); // ignored
    canvas_.setReadOnly(true);
    canvas_.removeColumn(columnId);
    EXPECT_EQ(document_.board().pages().columns().size(), 2U);
    canvas_.setReadOnly(false);
    canvas_.moveColumn(columnId, {});
    EXPECT_EQ(document_.board().pages().columns()[1].id, column);
    canvas_.removeColumn(columnId);
    EXPECT_EQ(document_.board().pages().columns().size(), 1U);
}

TEST(PageOverlayRows, ListHeadingsSubheadingsGapsAndNumbersInTheArea) {
    Board board;
    board.pages().start(paperA4, "Notes");
    const ColumnSpec spec = board.pages().columns()[0];
    const std::string first = spec.items[0].id;
    const std::string heading =
        board.pages().insertItem(spec.id, {}, ColumnItemKind::Subheading, "Part 2");
    const std::string second = board.pages().insertItem(spec.id, {}, ColumnItemKind::Page);
    PageMap map;
    map.refresh(board.pages());
    const std::vector<PageOverlayModel::Row> all =
        pageOverlayRows(map, board.pages().columns(), *map.bounds());
    const auto find = [&](const std::string &key) {
        return std::ranges::find(all, key, &PageOverlayModel::Row::key);
    };
    ASSERT_NE(find("h" + spec.id), all.end());
    EXPECT_EQ(find("h" + spec.id)->text, "Notes");
    ASSERT_NE(find("s" + heading), all.end());
    EXPECT_EQ(find("s" + heading)->text, "Part 2");
    EXPECT_EQ(find("p" + second)->number, 2);        // subheadings are not counted
    EXPECT_NE(find("g" + spec.id + ":"), all.end()); // the end of the column
    EXPECT_EQ(find("g" + spec.id + ":" + heading)->item, heading);
    EXPECT_EQ(all.size(), 1U + 1U + 2U + 4U); // heading, subheading, page numbers, gaps
    EXPECT_TRUE(std::ranges::is_sorted(all, {}, &PageOverlayModel::Row::key));
    const Bounds far{-9000.0F, -9000.0F, -8000.0F, -8000.0F};
    EXPECT_TRUE(pageOverlayRows(map, board.pages().columns(), far).empty());

    PageOverlayModel model;
    model.sync(all);
    EXPECT_EQ(model.rowCount(), static_cast<int>(all.size()));
    std::vector<PageOverlayModel::Row> fewer(all.begin(), all.begin() + 3);
    model.sync(fewer);
    EXPECT_EQ(model.rowCount(), 3);
    const int kind = model.roleNames().key("kind");
    EXPECT_FALSE(model.index(0).data(kind).toString().isEmpty());
}

TEST(PageStrokeNodes, KeepsNodesForPagesInViewAndRebuildsChangedOnes) {
    Board board;
    board.pages().start(paperA4, "");
    const std::string columnId = board.pages().columns()[0].id;
    const std::string first = board.pages().columns()[0].items[0].id;
    for (int i = 0; i < 20; ++i)
        board.pages().insertItem(columnId, {}, ColumnItemKind::Page);
    board.pages().addStroke(first, strokeAt(5.0F, 5.0F));
    PageMap map;
    map.refresh(board.pages());
    const Bounds firstPage = map.page(first)->bounds;
    QSGNode root;
    PageStrokeNodes nodes;
    const PageView near{firstPage, firstPage, 1.0F};
    nodes.update(&root, board, map, near, StrokeSelection::Pens);
    EXPECT_EQ(nodes.size(), 1U);
    EXPECT_EQ(root.childCount(), 1);
    const QSGNode *before = root.firstChild();
    const int oneStroke = meshVertices(before);
    EXPECT_GT(oneStroke, 0);
    nodes.update(&root, board, map, near, StrokeSelection::Pens);
    EXPECT_EQ(root.firstChild(), before); // nothing changed, nothing rebuilt
    board.pages().addStroke(first, strokeAt(5.0F, 50.0F));
    nodes.update(&root, board, map, near, StrokeSelection::Pens);
    EXPECT_EQ(root.childCount(), 1);
    EXPECT_EQ(meshVertices(root.firstChild()), 2 * oneStroke);
    const Bounds far{-9000.0F, -9000.0F, -8000.0F, -8000.0F};
    nodes.update(&root, board, map, {far, far, 1.0F}, StrokeSelection::Pens);
    EXPECT_EQ(root.childCount(), 0);
    nodes.update(&root, board, map, {near.show, near.keep, 0.001F}, StrokeSelection::Pens);
    EXPECT_EQ(root.childCount(), 0); // too small on screen to show strokes
}

} // namespace
} // namespace peerchalk
