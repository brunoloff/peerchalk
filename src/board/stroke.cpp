#include "board/stroke.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace peerchalk {

Stroke::Stroke(std::uint32_t argb, float width) : argb_(argb), width_(width) {
    if (!std::isfinite(width) || width <= 0.0F)
        throw std::invalid_argument("Stroke width must be finite and positive");
}

void Stroke::addPoint(StrokePoint point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        throw std::invalid_argument("Stroke point coordinates must be finite");
    if (!(point.pressure >= 0.0F && point.pressure <= 1.0F))
        throw std::invalid_argument("Stroke point pressure must be between 0 and 1");
    points_.push_back(point);
}

std::optional<Bounds> Stroke::bounds() const {
    if (points_.empty())
        return std::nullopt;
    const float half = width_ / 2.0F;
    Bounds result{points_.front().x, points_.front().y, points_.front().x, points_.front().y};
    for (const StrokePoint &point : points_) {
        result.left = std::min(result.left, point.x);
        result.top = std::min(result.top, point.y);
        result.right = std::max(result.right, point.x);
        result.bottom = std::max(result.bottom, point.y);
    }
    return Bounds{result.left - half, result.top - half, result.right + half, result.bottom + half};
}

} // namespace peerchalk
