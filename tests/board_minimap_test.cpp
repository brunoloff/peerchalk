#include "board/board_pages.hpp"
#include "ui/board_minimap.hpp"

#include <QQuickWindow>
#include <QTest>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

class BoardMinimapTest : public ::testing::Test {
  protected:
    void SetUp() override {
        canvas_.setParentItem(window_.contentItem());
        canvas_.setSize(QSizeF(200, 200));
        minimap_.setParentItem(window_.contentItem());
        minimap_.setPosition(QPointF(200, 0));
        minimap_.setSize(QSizeF(100, 100));
        window_.resize(300, 200);
        window_.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window_));
    }

    QQuickWindow window_;
    BoardCanvas canvas_;
    BoardMinimap minimap_;
};

TEST_F(BoardMinimapTest, HasNoFrameWithoutACanvas) { EXPECT_FALSE(minimap_.frame().has_value()); }

TEST_F(BoardMinimapTest, ClickCentresTheCanvasThere) {
    minimap_.setCanvas(&canvas_);
    QTest::mousePress(&window_, Qt::LeftButton, {}, {200, 0});
    QTest::mouseRelease(&window_, Qt::LeftButton, {}, {200, 0});
    EXPECT_LT(canvas_.viewCenter().x(), 100.0);
    EXPECT_LT(canvas_.viewCenter().y(), 100.0);
}

TEST_F(BoardMinimapTest, FrameShowsTheVisibleArea) {
    minimap_.setCanvas(&canvas_);
    const std::optional<Viewport> frame = minimap_.frame();
    ASSERT_TRUE(frame.has_value());
    const Bounds shown = frame->visibleWorld(100.0F, 100.0F);
    EXPECT_LE(shown.left, 0.0F);
    EXPECT_GE(shown.right, 200.0F);
}

TEST_F(BoardMinimapTest, FramesAPageBoardsPages) {
    BoardDocument document;
    document.board().pages().start(paperA4, "");
    canvas_.setDocument(&document);
    minimap_.setCanvas(&canvas_);
    const std::optional<Viewport> frame = minimap_.frame();
    ASSERT_TRUE(frame);
    // The whole page fits, though the canvas shows only its top left.
    const Point bottom = frame->toScreen({paperA4.width, paperA4.height});
    EXPECT_LE(bottom.x, 100.0F);
    EXPECT_LE(bottom.y, 100.0F);
}

} // namespace
} // namespace peerchalk
