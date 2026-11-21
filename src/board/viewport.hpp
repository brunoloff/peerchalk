#pragma once

#include "board/geometry.hpp"

#include <vector>

namespace peerchalk {

// Maps the infinite board's world coordinates to an item's screen coordinates:
// screen = world * scale + offset.
class Viewport {
  public:
    // Far enough out to see a column of hundreds of pages.
    static constexpr float minScale = 0.001F;
    static constexpr float maxScale = 8.0F;

    [[nodiscard]] float scale() const { return scale_; }
    [[nodiscard]] Point offset() const { return offset_; }
    [[nodiscard]] Point toWorld(Point screen) const;
    [[nodiscard]] Point toScreen(Point world) const;
    // The world area shown in a screen of the given size.
    [[nodiscard]] Bounds visibleWorld(float width, float height) const;

    // These throw std::invalid_argument for non-finite input.
    void panBy(float dx, float dy);
    // Scales by factor (> 0), clamped to [minScale, maxScale], keeping anchor fixed on screen.
    void zoomAt(Point anchor, float factor);
    void centerOn(Point world, float width, float height);
    // Shows all of content with margin pixels around it, never zooming in past maxFitScale.
    // It may zoom out past minScale, so that overviews such as the minimap fit anything.
    // Throws std::invalid_argument when the screen is not larger than twice the margin.
    void fit(const Bounds &content, float width, float height, float margin, float maxFitScale);

  private:
    float scale_{1.0F};
    Point offset_{0.0F, 0.0F};
};

// World distance between grid dots: doubles as the view zooms out so dots stay apart.
[[nodiscard]] float gridSpacing(float scale);
// Screen positions of the background grid dots visible in a screen of the given size.
[[nodiscard]] std::vector<Point> gridDots(const Viewport &viewport, float width, float height);

} // namespace peerchalk
