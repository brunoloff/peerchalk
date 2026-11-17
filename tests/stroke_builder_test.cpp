#include "board/stroke_builder.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr std::uint32_t black = 0xFF000000U;

TEST(StrokeBuilder, KeepsPressureAboveTheMinimum) {
    StrokeBuilder builder(black, 2.0F, StrokeKind::Pen, true);
    builder.add({0.0F, 0.0F}, 0.0F);
    builder.add({10.0F, 0.0F}, 0.5F);
    ASSERT_GE(builder.stroke().points().size(), 2U);
    EXPECT_FLOAT_EQ(builder.stroke().points().front().pressure, StrokeBuilder::minimumPressure);
    EXPECT_FLOAT_EQ(builder.stroke().points().back().pressure, 0.5F);
}

TEST(StrokeBuilder, WithoutASensorEveryPointHasFullPressure) {
    StrokeBuilder builder(black, 2.0F, StrokeKind::Pen, false);
    builder.add({0.0F, 0.0F}, 0.1F);
    builder.add({10.0F, 0.0F}, 0.3F);
    for (const StrokePoint &point : builder.stroke().points())
        EXPECT_FLOAT_EQ(point.pressure, 1.0F);
}

TEST(StrokeBuilder, HighlighterIgnoresPressure) {
    StrokeBuilder builder(black, 8.0F, StrokeKind::Highlighter, true);
    builder.add({0.0F, 0.0F}, 0.1F);
    builder.add({10.0F, 0.0F}, 0.9F);
    for (const StrokePoint &point : builder.stroke().points())
        EXPECT_FLOAT_EQ(point.pressure, 1.0F);
}

TEST(StrokeBuilder, DropsTheZeroPressureLiftOff) {
    StrokeBuilder builder(black, 2.0F, StrokeKind::Pen, true);
    builder.add({0.0F, 0.0F}, 0.5F);
    builder.add({10.0F, 0.0F}, 0.5F);
    builder.add({20.0F, 0.0F}, 0.0F);
    EXPECT_EQ(builder.stroke().points().size(), 2U);
}

TEST(StrokeBuilder, DropsSamplesTooCloseTogether) {
    StrokeBuilder builder(black, 2.0F, StrokeKind::Pen, false);
    builder.add({0.0F, 0.0F}, 1.0F);
    builder.add({10.0F, 0.0F}, 1.0F);
    builder.add({10.1F, 0.1F}, 1.0F);
    EXPECT_EQ(builder.stroke().points().size(), 2U);
}

TEST(StrokeBuilder, HeldDotGrowsToThePeakPressure) {
    StrokeBuilder builder(black, 4.0F, StrokeKind::Pen, true);
    builder.add({5.0F, 5.0F}, 0.2F);
    builder.add({5.1F, 5.0F}, 0.8F);
    builder.add({5.0F, 5.1F}, 0.4F);
    ASSERT_EQ(builder.stroke().points().size(), 1U);
    EXPECT_FLOAT_EQ(builder.stroke().points().front().pressure, 0.8F);
    EXPECT_FLOAT_EQ(builder.stroke().points().front().x, 5.0F);
}

TEST(StrokeBuilder, SmoothsLargeWidthJumps) {
    StrokeBuilder builder(black, 10.0F, StrokeKind::Pen, true);
    builder.add({0.0F, 0.0F}, 0.1F);
    builder.add({20.0F, 0.0F}, 1.0F);
    const std::vector<StrokePoint> &points = builder.stroke().points();
    ASSERT_GT(points.size(), 2U);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float step = (points[i].pressure - points[i - 1].pressure) * 10.0F;
        EXPECT_LE(step, StrokeBuilder::maximumWidthStep + 1e-4F);
        EXPECT_GT(points[i].x, points[i - 1].x);
    }
}

TEST(StrokeBuilder, ShortSegmentsAreNotOverSplit) {
    StrokeBuilder builder(black, 10.0F, StrokeKind::Pen, true);
    builder.add({0.0F, 0.0F}, 0.1F);
    builder.add({1.0F, 0.0F}, 1.0F);
    EXPECT_LE(builder.stroke().points().size(), 3U);
}

TEST(StrokeBuilder, RejectsNonFiniteSamples) {
    StrokeBuilder builder(black, 2.0F, StrokeKind::Pen, true);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(builder.add({nan, 0.0F}, 0.5F), std::invalid_argument);
    EXPECT_THROW(builder.add({0.0F, 0.0F}, nan), std::invalid_argument);
    EXPECT_TRUE(builder.empty());
}

} // namespace
} // namespace peerchalk
