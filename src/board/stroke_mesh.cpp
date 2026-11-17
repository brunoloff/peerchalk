#include "board/stroke_mesh.hpp"

#include <algorithm>
#include <cmath>

namespace peerchalk {
namespace {

// Pressure 0 still draws a hairline so that light touches remain visible.
constexpr float minimumHalfWidth = 0.25F;

float halfWidth(const Stroke &stroke, const StrokePoint &point) {
    return std::max(minimumHalfWidth, stroke.width() * point.pressure / 2.0F);
}

// Unit normal of the direction from a to b, or nullopt when they coincide.
std::optional<MeshVertex> unitNormal(const StrokePoint &a, const StrokePoint &b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = std::hypot(dx, dy);
    if (length == 0.0F)
        return std::nullopt;
    return MeshVertex{-dy / length, dx / length};
}

std::vector<MeshVertex> dot(const Stroke &stroke, const StrokePoint &point) {
    const float h = halfWidth(stroke, point);
    return {{point.x - h, point.y - h},
            {point.x - h, point.y + h},
            {point.x + h, point.y - h},
            {point.x + h, point.y + h}};
}

} // namespace

std::vector<MeshVertex> buildTriangleStrip(const Stroke &stroke) {
    const std::vector<StrokePoint> &points = stroke.points();
    if (points.empty())
        return {};
    if (points.size() == 1)
        return dot(stroke, points.front());
    std::vector<MeshVertex> strip;
    strip.reserve(points.size() * 2);
    std::optional<MeshVertex> normal;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const std::size_t next = std::min(i + 1, points.size() - 1);
        const std::size_t previous = i == next ? i - 1 : i;
        if (std::optional<MeshVertex> segment = unitNormal(points[previous], points[next]))
            normal = segment;
        if (!normal)
            continue;
        const float h = halfWidth(stroke, points[i]);
        strip.push_back({points[i].x + normal->x * h, points[i].y + normal->y * h});
        strip.push_back({points[i].x - normal->x * h, points[i].y - normal->y * h});
    }
    return strip.empty() ? dot(stroke, points.front()) : strip;
}

} // namespace peerchalk
