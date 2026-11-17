#include "ui/board_canvas.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

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

    void drawLine(QPoint from, QPoint to) {
        QTest::mousePress(&window_, Qt::LeftButton, {}, from);
        QTest::mouseMove(&window_, (from + to) / 2);
        QTest::mouseRelease(&window_, Qt::LeftButton, {}, to);
    }

    QQuickWindow window_;
    BoardCanvas canvas_;
};

TEST_F(BoardCanvasTest, MouseDragRecordsOneStroke) {
    QSignalSpy changed(&canvas_, &BoardCanvas::strokesChanged);
    canvas_.setPenColor(Qt::red);
    canvas_.setPenWidth(5.0);
    drawLine({10, 10}, {100, 50});
    ASSERT_EQ(canvas_.strokeCount(), 1);
    EXPECT_EQ(changed.count(), 1);
    const Stroke &stroke = canvas_.strokes().front();
    EXPECT_EQ(stroke.argb(), QColor(Qt::red).rgba());
    EXPECT_FLOAT_EQ(stroke.width(), 5.0F);
    ASSERT_GE(stroke.points().size(), 2U);
    EXPECT_FLOAT_EQ(stroke.points().front().x, 10.0F);
    EXPECT_FLOAT_EQ(stroke.points().back().y, 50.0F);
}

TEST_F(BoardCanvasTest, ClearRemovesStrokes) {
    drawLine({10, 10}, {20, 20});
    drawLine({30, 30}, {40, 40});
    ASSERT_EQ(canvas_.strokeCount(), 2);
    canvas_.clear();
    EXPECT_EQ(canvas_.strokeCount(), 0);
}

TEST_F(BoardCanvasTest, IgnoresInvalidPenSettings) {
    canvas_.setPenWidth(4.0);
    canvas_.setPenWidth(0.0);
    canvas_.setPenWidth(std::numeric_limits<qreal>::quiet_NaN());
    EXPECT_EQ(canvas_.penWidth(), 4.0);
    canvas_.setPenColor(Qt::blue);
    canvas_.setPenColor(QColor());
    EXPECT_EQ(canvas_.penColor(), QColor(Qt::blue));
}

} // namespace
} // namespace peerchalk

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
