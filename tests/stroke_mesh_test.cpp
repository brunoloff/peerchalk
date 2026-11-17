#include "board/stroke_mesh.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace peerchalk {
namespace {

TEST(StrokeMesh, EmptyStrokeHasNoVertices) {
    EXPECT_TRUE(buildTriangleStrip(Stroke(0xff000000U, 2.0F)).empty());
}

TEST(StrokeMesh, HorizontalLineIsOffsetVertically) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 10.0F, 1.0F});
    stroke.addPoint({20.0F, 10.0F, 1.0F});
    const std::vector<MeshVertex> strip = buildTriangleStrip(stroke);
    ASSERT_EQ(strip.size(), 4U);
    for (const MeshVertex &vertex : strip)
        EXPECT_FLOAT_EQ(std::abs(vertex.y - 10.0F), 2.0F);
    EXPECT_FLOAT_EQ(strip[0].x, 0.0F);
    EXPECT_FLOAT_EQ(strip[3].x, 20.0F);
}

TEST(StrokeMesh, PressureScalesTheWidth) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 0.0F, 0.5F});
    stroke.addPoint({10.0F, 0.0F, 0.5F});
    const std::vector<MeshVertex> strip = buildTriangleStrip(stroke);
    ASSERT_EQ(strip.size(), 4U);
    EXPECT_FLOAT_EQ(std::abs(strip[0].y), 1.0F);
}

TEST(StrokeMesh, SinglePointBecomesADot) {
    Stroke stroke(0xff000000U, 6.0F);
    stroke.addPoint({5.0F, 5.0F, 1.0F});
    const std::vector<MeshVertex> strip = buildTriangleStrip(stroke);
    ASSERT_EQ(strip.size(), 4U);
    EXPECT_FLOAT_EQ(strip[0].x, 2.0F);
    EXPECT_FLOAT_EQ(strip[3].y, 8.0F);
}

TEST(StrokeMesh, RepeatedPointsBecomeADot) {
    Stroke stroke(0xff000000U, 2.0F);
    stroke.addPoint({1.0F, 1.0F, 1.0F});
    stroke.addPoint({1.0F, 1.0F, 1.0F});
    EXPECT_EQ(buildTriangleStrip(stroke).size(), 4U);
}

TEST(StrokeMesh, ZeroPressureKeepsAHairline) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 0.0F, 0.0F});
    stroke.addPoint({10.0F, 0.0F, 0.0F});
    const std::vector<MeshVertex> strip = buildTriangleStrip(stroke);
    ASSERT_EQ(strip.size(), 4U);
    EXPECT_GT(std::abs(strip[0].y), 0.0F);
}

} // namespace
} // namespace peerchalk
