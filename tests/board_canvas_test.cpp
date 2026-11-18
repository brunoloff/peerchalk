#include "ui/board_canvas.hpp"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>
#include <QtGui/private/qeventpoint_p.h>

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
    // Announced once, on the next event-loop turn, by the board's document.
    QTest::qWait(50);
    EXPECT_EQ(changed.count(), 1);
    const Stroke &stroke = canvas_.board().strokes().front();
    EXPECT_EQ(stroke.argb(), QColor(Qt::red).rgba());
    EXPECT_FLOAT_EQ(stroke.width(), 5.0F);
    EXPECT_FLOAT_EQ(stroke.points().front().x, 10.0F);
    EXPECT_FLOAT_EQ(stroke.points().back().y, 50.0F);
}

TEST_F(BoardCanvasTest, ReadOnlyBoardsCannotBeChangedButCanBeMoved) {
    drag({10, 10}, {100, 50});
    ASSERT_EQ(canvas_.strokeCount(), 1);
    canvas_.setReadOnly(true);
    drag({20, 20}, {120, 60});
    EXPECT_EQ(canvas_.strokeCount(), 1);
    canvas_.undo();
    EXPECT_EQ(canvas_.strokeCount(), 1);
    canvas_.setTool(BoardCanvas::Tool::Eraser);
    drag({10, 10}, {100, 50});
    EXPECT_EQ(canvas_.strokeCount(), 1);
    const QPointF center = canvas_.viewCenter();
    drag({50, 50}, {90, 90}, Qt::RightButton);
    EXPECT_NE(canvas_.viewCenter(), center);
    canvas_.setReadOnly(false);
    canvas_.undo();
    EXPECT_EQ(canvas_.strokeCount(), 0);
}

TEST_F(BoardCanvasTest, HighlighterIsWideWithConstantWidth) {
    canvas_.setTool(BoardCanvas::Tool::Highlighter);
    canvas_.setPenColor(Qt::yellow);
    canvas_.setPenWidth(4.0);
    drag({10, 10}, {100, 10});
    ASSERT_EQ(canvas_.strokeCount(), 1);
    const Stroke &stroke = canvas_.board().strokes().front();
    EXPECT_EQ(stroke.kind(), StrokeKind::Highlighter);
    EXPECT_EQ(stroke.argb(), QColor(Qt::yellow).rgba());
    EXPECT_FLOAT_EQ(stroke.width(), 16.0F);
    for (const StrokePoint &point : stroke.points())
        EXPECT_FLOAT_EQ(point.pressure, 1.0F);
}

// Sends the mouse events Qt synthesises from a pen on a tablet: they carry the pen's
// device, which reports pressure.
TEST_F(BoardCanvasTest, PenPressureReachesTheStroke) {
    QPointingDevice pen(
        "test pen", 4242, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 3);
    const struct {
        QEvent::Type type;
        QPointF position;
        qreal pressure;
    } samples[] = {{QEvent::MouseButtonPress, {20, 20}, 0.2},
                   {QEvent::MouseMove, {60, 20}, 0.6},
                   {QEvent::MouseMove, {100, 20}, 0.9},
                   {QEvent::MouseButtonRelease, {100, 20}, 0.0}};
    for (const auto &sample : samples) {
        const bool release = sample.type == QEvent::MouseButtonRelease;
        QMouseEvent event(sample.type, sample.position, sample.position,
                          window_.mapToGlobal(sample.position), Qt::LeftButton,
                          release ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier, &pen);
        QMutableEventPoint::setPressure(event.point(0), sample.pressure);
        QGuiApplication::sendEvent(&window_, &event);
    }
    ASSERT_EQ(canvas_.strokeCount(), 1);
    const Stroke &stroke = canvas_.board().strokes().front();
    EXPECT_FLOAT_EQ(stroke.points().front().pressure, 0.2F);
    EXPECT_FLOAT_EQ(stroke.points().back().pressure, 0.9F);
}

TEST_F(BoardCanvasTest, ShowsChangesMadeToItsDocumentElsewhere) {
    BoardDocument document;
    canvas_.setDocument(&document);
    QSignalSpy changed(&canvas_, &BoardCanvas::strokesChanged);
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({5.0F, 5.0F, 1.0F});
    document.board().addStroke(stroke); // e.g. a peer's update applied by sync
    ASSERT_TRUE(QTest::qWaitFor([&] { return changed.count() > 0; }));
    EXPECT_EQ(canvas_.strokeCount(), 1);
    canvas_.setDocument(nullptr);
    EXPECT_EQ(canvas_.strokeCount(), 0);
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

TEST_F(BoardCanvasTest, RightButtonPansWhileUsingThePen) {
    drag({100, 100}, {100, 40}, Qt::RightButton);
    EXPECT_EQ(canvas_.strokeCount(), 0);
    EXPECT_DOUBLE_EQ(canvas_.viewCenter().y(), 160.0);
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
