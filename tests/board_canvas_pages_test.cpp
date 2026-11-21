#include "board/board_pages.hpp"
#include "ui/board_canvas.hpp"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

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
        canvas_.setSize(QSizeF(300, 300));
        canvas_.setDocument(&document_);
        window_.resize(300, 300);
        window_.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window_));
        ASSERT_TRUE(canvas_.pageBoard());
        top_ = canvas_.pageMap().page(page_)->bounds.top;
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
    float top_{0.0F}; // the first page's top; the view starts at the board's origin
};

TEST_F(BoardCanvasPagesTest, DrawsOnThePageUnderTheStrokeInPageCoordinates) {
    const int y = static_cast<int>(top_) + 40;
    drag({50, y}, {120, y + 20});
    ASSERT_EQ(pageStrokes().size(), 1U);
    EXPECT_TRUE(document_.board().strokes().empty());
    EXPECT_FLOAT_EQ(pageStrokes()[0].points().front().x, 50.0F);
    EXPECT_FLOAT_EQ(pageStrokes()[0].points().front().y, 40.0F);
}

TEST_F(BoardCanvasPagesTest, DropsStrokesStartedBetweenPages) {
    drag({50, 20}, {80, static_cast<int>(top_) + 40}); // starts in the heading band
    EXPECT_TRUE(pageStrokes().empty());
}

TEST_F(BoardCanvasPagesTest, ErasesPageStrokesAndUndoBringsThemBack) {
    document_.board().pages().addStroke(page_, strokeAt(40.0F, 30.0F));
    canvas_.setTool(BoardCanvas::Tool::Eraser);
    drag({45, static_cast<int>(top_) + 30}, {46, static_cast<int>(top_) + 30});
    EXPECT_TRUE(pageStrokes().empty());
    canvas_.undo();
    EXPECT_EQ(pageStrokes().size(), 1U);
}

TEST_F(BoardCanvasPagesTest, PutsTextBoxesOnPagesAndMovesThemInPageCoordinates) {
    canvas_.setTool(BoardCanvas::Tool::Text);
    QTest::mouseClick(&window_, Qt::LeftButton, {}, {60, static_cast<int>(top_) + 50});
    ASSERT_EQ(document_.board().textBoxes().size(), 1U);
    const TextBox box = document_.board().textBoxes()[0];
    EXPECT_EQ(box.page, page_);
    EXPECT_FLOAT_EQ(box.x, 60.0F);
    const QModelIndex row = canvas_.textBoxes()->index(0, 0);
    const int xRole = canvas_.textBoxes()->roleNames().key("boxX");
    EXPECT_FLOAT_EQ(row.data(xRole).toFloat(), 60.0F); // the model shows board positions
    canvas_.moveTextBox(QString::fromStdString(box.id), QPointF(70.0, top_ + 90.0));
    EXPECT_FLOAT_EQ(document_.board().textBox(box.id)->y, 90.0F);
    canvas_.finishTextEditing();                               // an empty box goes away
    QTest::mouseClick(&window_, Qt::LeftButton, {}, {60, 20}); // between pages: no box
    EXPECT_TRUE(document_.board().textBoxes().empty());
}

TEST_F(BoardCanvasPagesTest, FitsThePagesAndReportsTheirBounds) {
    const QRectF bounds = canvas_.contentBounds();
    EXPECT_FLOAT_EQ(static_cast<float>(bounds.width()), paperA4.width);
    canvas_.fitContent();
    EXPECT_LT(canvas_.zoom(), 1.0);
    EXPECT_TRUE(canvas_.visibleArea().contains(bounds.center()));
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
