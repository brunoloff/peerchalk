#include "board/viewport.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

TEST(Viewport, StartsAtIdentity) {
    const Viewport viewport;
    const Point screen = viewport.toScreen({12.0F, -7.0F});
    EXPECT_FLOAT_EQ(screen.x, 12.0F);
    EXPECT_FLOAT_EQ(screen.y, -7.0F);
}

TEST(Viewport, PanMovesTheWorld) {
    Viewport viewport;
    viewport.panBy(30.0F, -10.0F);
    const Point world = viewport.toWorld({30.0F, -10.0F});
    EXPECT_FLOAT_EQ(world.x, 0.0F);
    EXPECT_FLOAT_EQ(world.y, 0.0F);
}

TEST(Viewport, ZoomKeepsTheAnchorFixed) {
    Viewport viewport;
    viewport.panBy(17.0F, 5.0F);
    const Point before = viewport.toWorld({100.0F, 60.0F});
    viewport.zoomAt({100.0F, 60.0F}, 2.5F);
    const Point after = viewport.toWorld({100.0F, 60.0F});
    EXPECT_FLOAT_EQ(viewport.scale(), 2.5F);
    EXPECT_NEAR(after.x, before.x, 1e-4);
    EXPECT_NEAR(after.y, before.y, 1e-4);
}

TEST(Viewport, ZoomIsClamped) {
    Viewport viewport;
    viewport.zoomAt({0.0F, 0.0F}, 1000.0F);
    EXPECT_FLOAT_EQ(viewport.scale(), Viewport::maxScale);
    viewport.zoomAt({0.0F, 0.0F}, 1e-6F);
    EXPECT_FLOAT_EQ(viewport.scale(), Viewport::minScale);
}

TEST(Viewport, RejectsInvalidInput) {
    Viewport viewport;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(viewport.panBy(nan, 0.0F), std::invalid_argument);
    EXPECT_THROW(viewport.zoomAt({0.0F, 0.0F}, 0.0F), std::invalid_argument);
    EXPECT_THROW(viewport.zoomAt({nan, 0.0F}, 2.0F), std::invalid_argument);
    EXPECT_THROW(viewport.centerOn({0.0F, nan}, 10.0F, 10.0F), std::invalid_argument);
    EXPECT_THROW(viewport.fit({0, 0, 10, 10}, 20.0F, 20.0F, 10.0F, 1.0F), std::invalid_argument);
    EXPECT_FLOAT_EQ(viewport.scale(), 1.0F);
}

TEST(Viewport, FitShowsAllContentCentred) {
    Viewport viewport;
    viewport.fit({1000.0F, 1000.0F, 1400.0F, 1100.0F}, 200.0F, 100.0F, 0.0F, 1.0F);
    EXPECT_FLOAT_EQ(viewport.scale(), 0.5F);
    const Bounds visible = viewport.visibleWorld(200.0F, 100.0F);
    EXPECT_NEAR(visible.left, 1000.0F, 1e-3);
    EXPECT_NEAR(visible.right, 1400.0F, 1e-3);
    EXPECT_NEAR((visible.top + visible.bottom) / 2.0F, 1050.0F, 1e-3);
}

TEST(Viewport, FitDoesNotZoomPastTheLimit) {
    Viewport viewport;
    viewport.fit({0.0F, 0.0F, 10.0F, 10.0F}, 500.0F, 500.0F, 0.0F, 1.0F);
    EXPECT_FLOAT_EQ(viewport.scale(), 1.0F);
}

TEST(Viewport, FitGoesFurtherOutThanZoomingWhenItMust) {
    Viewport viewport;
    viewport.fit({0.0F, 0.0F, 1000.0F, 600000.0F}, 150.0F, 150.0F, 0.0F, 1.0F);
    EXPECT_FLOAT_EQ(viewport.scale(), 150.0F / 600000.0F); // a 500-page column in a minimap
}

TEST(Grid, SpacingGrowsWhenZoomedOut) {
    EXPECT_FLOAT_EQ(gridSpacing(1.0F), 24.0F);
    EXPECT_FLOAT_EQ(gridSpacing(0.25F), 48.0F);
    EXPECT_GE(gridSpacing(Viewport::minScale) * Viewport::minScale, 12.0F);
}

TEST(Grid, DotsCoverTheScreenAndStayBounded) {
    Viewport viewport;
    const std::vector<Point> dots = gridDots(viewport, 240.0F, 120.0F);
    EXPECT_EQ(dots.size(), 11U * 6U);
    viewport.zoomAt({0.0F, 0.0F}, Viewport::minScale);
    EXPECT_LE(gridDots(viewport, 4000.0F, 4000.0F).size(), 80000U);
}

} // namespace
} // namespace peerchalk
