#include "board/viewport.hpp"

#include <cmath>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr float baseGridSpacing = 24.0F;
constexpr float minDotGapPx = 12.0F;
constexpr std::size_t maxGridDots = 80000; // about a 4K screen at the densest spacing
constexpr float minFitScale = 1e-6F;

void requireFinite(Point point, const char *what) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        throw std::invalid_argument(what);
}

} // namespace

Point Viewport::toWorld(Point screen) const {
    return {(screen.x - offset_.x) / scale_, (screen.y - offset_.y) / scale_};
}

Point Viewport::toScreen(Point world) const {
    return {world.x * scale_ + offset_.x, world.y * scale_ + offset_.y};
}

Bounds Viewport::visibleWorld(float width, float height) const {
    const Point topLeft = toWorld({0.0F, 0.0F});
    const Point bottomRight = toWorld({width, height});
    return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

void Viewport::panBy(float dx, float dy) {
    requireFinite({dx, dy}, "Viewport pan distance must be finite");
    offset_ = {offset_.x + dx, offset_.y + dy};
}

void Viewport::zoomAt(Point anchor, float factor) {
    requireFinite(anchor, "Viewport zoom anchor must be finite");
    if (!std::isfinite(factor) || factor <= 0.0F)
        throw std::invalid_argument("Viewport zoom factor must be finite and positive");
    const Point world = toWorld(anchor);
    scale_ = std::clamp(scale_ * factor, minScale, maxScale);
    offset_ = {anchor.x - world.x * scale_, anchor.y - world.y * scale_};
}

void Viewport::centerOn(Point world, float width, float height) {
    requireFinite(world, "Viewport centre must be finite");
    requireFinite({width, height}, "Viewport size must be finite");
    offset_ = {width / 2.0F - world.x * scale_, height / 2.0F - world.y * scale_};
}

void Viewport::fit(const Bounds &content, float width, float height, float margin,
                   float maxFitScale) {
    if (!(width > 2.0F * margin) || !(height > 2.0F * margin))
        throw std::invalid_argument("Viewport is too small to fit content");
    const float contentWidth = std::max(content.width(), 1.0F);
    const float contentHeight = std::max(content.height(), 1.0F);
    const float fitted = std::min({(width - 2.0F * margin) / contentWidth,
                                   (height - 2.0F * margin) / contentHeight, maxFitScale});
    scale_ = std::clamp(fitted, minFitScale, maxScale);
    centerOn({(content.left + content.right) / 2.0F, (content.top + content.bottom) / 2.0F}, width,
             height);
}

float gridSpacing(float scale) {
    float spacing = baseGridSpacing;
    while (spacing * scale < minDotGapPx)
        spacing *= 2.0F;
    return spacing;
}

std::vector<Point> gridDots(const Viewport &viewport, float width, float height) {
    const float spacing = gridSpacing(viewport.scale());
    const Bounds area = viewport.visibleWorld(width, height);
    const float firstX = std::floor(area.left / spacing) * spacing;
    const float firstY = std::floor(area.top / spacing) * spacing;
    const auto columns = static_cast<std::size_t>((area.right - firstX) / spacing) + 1;
    const auto rows = static_cast<std::size_t>((area.bottom - firstY) / spacing) + 1;
    if (columns * rows > maxGridDots)
        return {};
    std::vector<Point> dots;
    dots.reserve(columns * rows);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            dots.push_back(viewport.toScreen({firstX + static_cast<float>(column) * spacing,
                                              firstY + static_cast<float>(row) * spacing}));
        }
    }
    return dots;
}

} // namespace peerchalk
