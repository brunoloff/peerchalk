#include "board/stroke.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

TEST(Stroke, KeepsPointsInOrder) {
    Stroke stroke(0xff000000U, 2.0F);
    stroke.addPoint({1.0F, 2.0F, 0.5F});
    stroke.addPoint({3.0F, 4.0F, 1.0F});
    ASSERT_EQ(stroke.points().size(), 2U);
    EXPECT_FLOAT_EQ(stroke.points()[1].x, 3.0F);
    EXPECT_FLOAT_EQ(stroke.points()[1].pressure, 1.0F);
}

TEST(Stroke, BoundsIncludeHalfTheWidth) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({10.0F, 20.0F, 1.0F});
    stroke.addPoint({30.0F, 5.0F, 1.0F});
    const std::optional<Bounds> bounds = stroke.bounds();
    ASSERT_TRUE(bounds.has_value());
    EXPECT_FLOAT_EQ(bounds->left, 8.0F);
    EXPECT_FLOAT_EQ(bounds->top, 3.0F);
    EXPECT_FLOAT_EQ(bounds->right, 32.0F);
    EXPECT_FLOAT_EQ(bounds->bottom, 22.0F);
}

TEST(Stroke, EmptyStrokeHasNoBounds) {
    EXPECT_FALSE(Stroke(0xff000000U, 1.0F).bounds().has_value());
}

TEST(Stroke, RejectsInvalidWidth) {
    EXPECT_THROW(Stroke(0xff000000U, 0.0F), std::invalid_argument);
    EXPECT_THROW(Stroke(0xff000000U, -1.0F), std::invalid_argument);
    EXPECT_THROW(Stroke(0xff000000U, std::numeric_limits<float>::infinity()),
                 std::invalid_argument);
}

TEST(Stroke, RejectsInvalidPoints) {
    Stroke stroke(0xff000000U, 1.0F);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(stroke.addPoint({nan, 0.0F, 1.0F}), std::invalid_argument);
    EXPECT_THROW(stroke.addPoint({0.0F, 0.0F, 1.5F}), std::invalid_argument);
    EXPECT_THROW(stroke.addPoint({0.0F, 0.0F, -0.1F}), std::invalid_argument);
    EXPECT_THROW(stroke.addPoint({0.0F, 0.0F, nan}), std::invalid_argument);
    EXPECT_TRUE(stroke.points().empty());
}

} // namespace
} // namespace peerchalk
