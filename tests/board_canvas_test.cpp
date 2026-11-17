#include "ui/board_canvas.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

#include <gtest/gtest.h>

#include <limits>

namespace peerchalk {
namespace {

class BoardCanvasTest : public ::testing::Test {
  protected:
    void SetUp() override {
        canvas_.setParentItem(window_.contentItem());
        canvas_.setSize(QSizeF(200, 200));
        window_.resize(200, 200);
        window_.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window_));
    }

    void drag(QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton) {
        QTest::mousePress(&window_, button, {}, from);
        QTest::mouseMove(&window_, (from + to) / 2);
        QTest::mouseRelease(&window_, button, {}, to);
    }

    void wheel(QPointF at, QPoint angleDelta, Qt::KeyboardModifiers modifiers) {
        QWheelEvent event(at, window_.mapToGlobal(at), QPoint(), angleDelta, Qt::NoButton,
                          modifiers, Qt::NoScrollPhase, false);
        QGuiApplication::sendEvent(&window_, &event);
    }

    QQuickWindow window_;
    BoardCanvas canvas_;
};

TEST_F(BoardCanvasTest, PenDragRecordsOneStroke) {
    QSignalSpy changed(&canvas_, &BoardCanvas::strokesChanged);
    canvas_.setPenColor(Qt::red);
    canvas_.setPenWidth(5.0);
    drag({10, 10}, {100, 50});
    ASSERT_EQ(canvas_.strokeCount(), 1);
    EXPECT_EQ(changed.count(), 1);
    const Stroke &stroke = canvas_.board().strokes().front();
    EXPECT_EQ(stroke.argb(), QColor(Qt::red).rgba());
    EXPECT_FLOAT_EQ(stroke.width(), 5.0F);
    EXPECT_FLOAT_EQ(stroke.points().front().x, 10.0F);
    EXPECT_FLOAT_EQ(stroke.points().back().y, 50.0F);
}

TEST_F(BoardCanvasTest, HighlighterIsTranslucentAndWide) {
    canvas_.setTool(BoardCanvas::Tool::Highlighter);
    canvas_.setPenWidth(4.0);
    drag({10, 10}, {100, 10});
    ASSERT_EQ(canvas_.strokeCount(), 1);
    const Stroke &stroke = canvas_.board().strokes().front();
    EXPECT_LT(qAlpha(stroke.argb()), 255);
    EXPECT_FLOAT_EQ(stroke.width(), 16.0F);
}

TEST_F(BoardCanvasTest, UndoRemovesTheLastStroke) {
    drag({10, 10}, {20, 20});
    drag({30, 30}, {40, 40});
    ASSERT_TRUE(canvas_.canUndo());
    canvas_.undo();
    EXPECT_EQ(canvas_.strokeCount(), 1);
}

TEST_F(BoardCanvasTest, PanToolMovesTheViewNotTheBoard) {
    canvas_.setTool(BoardCanvas::Tool::Pan);
    drag({100, 100}, {60, 80});
    EXPECT_EQ(canvas_.strokeCount(), 0);
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().x(), 140.0);
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().y(), 120.0);
}

TEST_F(BoardCanvasTest, MiddleButtonPansWhileUsingThePen) {
    drag({100, 100}, {150, 100}, Qt::MiddleButton);
    EXPECT_EQ(canvas_.strokeCount(), 0);
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().x(), 50.0);
}

TEST_F(BoardCanvasTest, EraserRemovesStrokesAndUndoRestoresThem) {
    drag({20, 50}, {180, 50});
    drag({20, 150}, {180, 150});
    canvas_.setTool(BoardCanvas::Tool::Eraser);
    drag({100, 30}, {100, 170});
    EXPECT_EQ(canvas_.strokeCount(), 0);
    canvas_.undo();
    EXPECT_EQ(canvas_.strokeCount(), 2);
}

TEST_F(BoardCanvasTest, CtrlWheelZoomsAroundTheCursor) {
    wheel({50, 50}, {0, 120}, Qt::ControlModifier);
    const qreal zoom = canvas_.zoom();
    EXPECT_GT(zoom, 1.0);
    EXPECT_NEAR(canvas_.visibleArea().left(), 50.0 - 50.0 / zoom, 1e-3);
    EXPECT_NEAR(canvas_.visibleArea().top(), 50.0 - 50.0 / zoom, 1e-3);
}

TEST_F(BoardCanvasTest, PlainWheelPans) {
    wheel({50, 50}, {0, -120}, Qt::NoModifier);
    EXPECT_DOUBLE_EQ(canvas_.zoom(), 1.0);
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().y(), 160.0);
}

TEST_F(BoardCanvasTest, FitContentShowsEveryStroke) {
    drag({20, 20}, {60, 60});
    canvas_.centerOn(QPointF(5000, -5000));
    ASSERT_FALSE(canvas_.visibleArea().intersects(canvas_.contentBounds()));
    canvas_.fitContent();
    EXPECT_TRUE(canvas_.visibleArea().contains(canvas_.contentBounds()));
}

TEST_F(BoardCanvasTest, FitContentOnEmptyBoardKeepsTheView) {
    canvas_.fitContent();
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().x(), 100.0);
    EXPECT_DOUBLE_EQ(canvas_.zoom(), 1.0);
}

TEST_F(BoardCanvasTest, IgnoresInvalidSettings) {
    canvas_.setPenWidth(4.0);
    canvas_.setPenWidth(0.0);
    canvas_.setPenWidth(std::numeric_limits<qreal>::quiet_NaN());
    EXPECT_EQ(canvas_.penWidth(), 4.0);
    canvas_.setPenColor(Qt::blue);
    canvas_.setPenColor(QColor());
    EXPECT_EQ(canvas_.penColor(), QColor(Qt::blue));
    canvas_.zoomBy(-2.0);
    canvas_.zoomBy(std::numeric_limits<qreal>::infinity());
    EXPECT_DOUBLE_EQ(canvas_.zoom(), 1.0);
}

} // namespace
} // namespace peerchalk
