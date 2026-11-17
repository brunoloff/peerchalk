#include "board/board.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

Stroke line(float x0, float y0, float x1, float y1, std::uint32_t argb = 0xff000000U) {
    Stroke stroke(argb, 2.0F);
    stroke.addPoint({x0, y0, 1.0F});
    stroke.addPoint({x1, y1, 1.0F});
    return stroke;
}

TEST(StrokeNear, HitsAlongTheSegmentIncludingWidth) {
    const Stroke stroke = line(0.0F, 0.0F, 100.0F, 0.0F);
    EXPECT_TRUE(strokeNear(stroke, {50.0F, 3.0F}, 2.5F));
    EXPECT_FALSE(strokeNear(stroke, {50.0F, 10.0F}, 2.5F));
    EXPECT_FALSE(strokeNear(stroke, {120.0F, 0.0F}, 5.0F));
}

TEST(StrokeNear, HandlesSinglePointStrokes) {
    Stroke dot(0xff000000U, 4.0F);
    dot.addPoint({5.0F, 5.0F, 1.0F});
    EXPECT_TRUE(strokeNear(dot, {7.0F, 5.0F}, 0.5F));
    EXPECT_FALSE(strokeNear(dot, {12.0F, 5.0F}, 0.5F));
}

TEST(Board, UndoRemovesTheLastStroke) {
    Board board;
    board.addStroke(line(0, 0, 10, 0));
    board.addStroke(line(0, 20, 10, 20));
    ASSERT_TRUE(board.undo());
    ASSERT_EQ(board.strokes().size(), 1U);
    EXPECT_FLOAT_EQ(board.strokes().front().points().front().y, 0.0F);
}

TEST(Board, UndoOnEmptyHistoryDoesNothing) {
    Board board;
    EXPECT_FALSE(board.canUndo());
    EXPECT_FALSE(board.undo());
}

TEST(Board, OneEraseGestureIsOneUndoStep) {
    Board board;
    board.addStroke(line(0, 0, 10, 0, 1U));
    board.addStroke(line(0, 50, 10, 50, 2U));
    board.addStroke(line(0, 100, 10, 100, 3U));
    EXPECT_EQ(board.eraseNear({5.0F, 0.0F}, 2.0F), 1U);
    EXPECT_EQ(board.eraseNear({5.0F, 100.0F}, 2.0F), 1U);
    board.endGesture();
    ASSERT_EQ(board.strokes().size(), 1U);
    ASSERT_TRUE(board.undo());
    ASSERT_EQ(board.strokes().size(), 3U);
    EXPECT_EQ(board.strokes()[0].argb(), 1U);
    EXPECT_EQ(board.strokes()[1].argb(), 2U);
    EXPECT_EQ(board.strokes()[2].argb(), 3U);
}

TEST(Board, SeparateEraseGesturesUndoSeparately) {
    Board board;
    board.addStroke(line(0, 0, 10, 0));
    board.addStroke(line(0, 50, 10, 50));
    board.eraseNear({5.0F, 0.0F}, 2.0F);
    board.endGesture();
    board.eraseNear({5.0F, 50.0F}, 2.0F);
    board.endGesture();
    ASSERT_TRUE(board.undo());
    EXPECT_EQ(board.strokes().size(), 1U);
}

TEST(Board, EraseMissChangesNothing) {
    Board board;
    board.addStroke(line(0, 0, 10, 0));
    EXPECT_EQ(board.eraseNear({500.0F, 500.0F}, 2.0F), 0U);
    EXPECT_EQ(board.strokes().size(), 1U);
}

TEST(Board, RejectsInvalidEraser) {
    Board board;
    EXPECT_THROW(board.eraseNear({0.0F, 0.0F}, -1.0F), std::invalid_argument);
    EXPECT_THROW(board.eraseNear({std::numeric_limits<float>::infinity(), 0.0F}, 1.0F),
                 std::invalid_argument);
}

TEST(Board, ContentBoundsCoverAllStrokes) {
    Board board;
    EXPECT_FALSE(board.contentBounds().has_value());
    board.addStroke(line(0, 0, 10, 0));
    board.addStroke(line(-40, 30, -20, 60));
    const std::optional<Bounds> bounds = board.contentBounds();
    ASSERT_TRUE(bounds.has_value());
    EXPECT_FLOAT_EQ(bounds->left, -41.0F);
    EXPECT_FLOAT_EQ(bounds->bottom, 61.0F);
}

} // namespace
} // namespace peerchalk
