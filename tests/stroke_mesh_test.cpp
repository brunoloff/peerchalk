#include "board/stroke_mesh.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace peerchalk {
namespace {

Bounds meshBounds(const std::vector<MeshVertex> &mesh) {
    Bounds bounds{mesh.front().x, mesh.front().y, mesh.front().x, mesh.front().y};
    for (const MeshVertex &v : mesh)
        bounds = unite(bounds, {v.x, v.y, v.x, v.y});
    return bounds;
}

TEST(StrokeMesh, EmptyStrokeHasNoTriangles) {
    EXPECT_TRUE(buildStrokeMesh(Stroke(0xff000000U, 2.0F)).empty());
}

TEST(StrokeMesh, MeshIsMadeOfWholeTriangles) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 0.0F, 1.0F});
    stroke.addPoint({20.0F, 5.0F, 0.5F});
    stroke.addPoint({30.0F, -5.0F, 0.8F});
    EXPECT_EQ(buildStrokeMesh(stroke).size() % 3, 0U);
}

TEST(StrokeMesh, LineHasRoundCapsOfHalfTheWidth) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 10.0F, 1.0F});
    stroke.addPoint({20.0F, 10.0F, 1.0F});
    const Bounds bounds = meshBounds(buildStrokeMesh(stroke));
    EXPECT_NEAR(bounds.left, -2.0F, 1e-4);
    EXPECT_NEAR(bounds.right, 22.0F, 1e-4);
    EXPECT_NEAR(bounds.top, 8.0F, 1e-4);
    EXPECT_NEAR(bounds.bottom, 12.0F, 1e-4);
}

TEST(StrokeMesh, PressureScalesTheWidth) {
    Stroke stroke(0xff000000U, 8.0F);
    stroke.addPoint({0.0F, 0.0F, 0.25F});
    stroke.addPoint({50.0F, 0.0F, 0.25F});
    const Bounds bounds = meshBounds(buildStrokeMesh(stroke));
    EXPECT_NEAR(bounds.bottom - bounds.top, 2.0F, 1e-4);
}

TEST(StrokeMesh, SinglePointIsARoundDot) {
    Stroke stroke(0xff000000U, 6.0F);
    stroke.addPoint({5.0F, 5.0F, 1.0F});
    const std::vector<MeshVertex> mesh = buildStrokeMesh(stroke);
    ASSERT_FALSE(mesh.empty());
    for (const MeshVertex &v : mesh)
        EXPECT_LE(std::hypot(v.x - 5.0F, v.y - 5.0F), 3.0F + 1e-4F);
    const Bounds bounds = meshBounds(mesh);
    EXPECT_NEAR(bounds.right - bounds.left, 6.0F, 1e-3);
}

TEST(StrokeMesh, ZeroPressureKeepsAHairline) {
    Stroke stroke(0xff000000U, 4.0F);
    stroke.addPoint({0.0F, 0.0F, 0.0F});
    stroke.addPoint({10.0F, 0.0F, 0.0F});
    EXPECT_GT(pointWidth(stroke, stroke.points().front()), 0.0F);
    const Bounds bounds = meshBounds(buildStrokeMesh(stroke));
    EXPECT_GT(bounds.bottom - bounds.top, 0.0F);
}

TEST(StrokeMesh, JointDiscUsesTheWiderNeighbour) {
    Stroke stroke(0xff000000U, 10.0F);
    stroke.addPoint({0.0F, 0.0F, 1.0F});
    stroke.addPoint({10.0F, 0.0F, 0.2F});
    stroke.addPoint({20.0F, 0.0F, 0.2F});
    const std::vector<MeshVertex> mesh = buildStrokeMesh(stroke);
    const float reachAtJoint = std::ranges::max(mesh, {}, [](const MeshVertex &v) {
                                   return std::abs(v.x - 10.0F) < 1e-3F ? v.y : -1.0F;
                               }).y;
    EXPECT_NEAR(reachAtJoint, 5.0F, 1e-3);
}

TEST(StrokeMesh, SimplifiedStrokesKeepTheirEndsAndSpacedPoints) {
    Stroke stroke(0xFF000000U, 3.0F, StrokeKind::Highlighter);
    for (int i = 0; i <= 10; ++i)
        stroke.addPoint({static_cast<float>(i), 0.0F, 1.0F});
    const Stroke simple = simplifiedStroke(stroke, 4.0F);
    ASSERT_EQ(simple.points().size(), 4U); // 0, 4, 8 and the end
    EXPECT_FLOAT_EQ(simple.points()[1].x, 4.0F);
    EXPECT_FLOAT_EQ(simple.points().back().x, 10.0F);
    EXPECT_EQ(simple.kind(), StrokeKind::Highlighter);
    EXPECT_FLOAT_EQ(simple.width(), 3.0F);
    EXPECT_EQ(simplifiedStroke(stroke, 0.0F).points().size(), 11U);
    EXPECT_TRUE(simplifiedStroke(Stroke(0U, 1.0F), 4.0F).points().empty());
    EXPECT_THROW((void)simplifiedStroke(stroke, -1.0F), std::invalid_argument);
}

} // namespace
} // namespace peerchalk
