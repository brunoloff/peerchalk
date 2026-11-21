#include "board/stroke_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace peerchalk {
namespace {

// Pressure 0 still draws a hairline so that light touches remain visible.
constexpr float minimumWidth = 0.5F;
// Discs use enough sides that each is at most this long in world units.
constexpr float discSideLength = 2.0F;
constexpr int minDiscSides = 8;
constexpr int maxDiscSides = 48;

void appendDisc(std::vector<MeshVertex> &mesh, const StrokePoint &centre, float radius) {
    const float circumference = 2.0F * std::numbers::pi_v<float> * radius;
    const int sides = std::clamp(static_cast<int>(std::ceil(circumference / discSideLength)),
                                 minDiscSides, maxDiscSides);
    const float step = 2.0F * std::numbers::pi_v<float> / static_cast<float>(sides);
    for (int i = 0; i < sides; ++i) {
        const float a0 = step * static_cast<float>(i);
        const float a1 = step * static_cast<float>(i + 1);
        mesh.push_back({centre.x, centre.y});
        mesh.push_back({centre.x + radius * std::cos(a0), centre.y + radius * std::sin(a0)});
        mesh.push_back({centre.x + radius * std::cos(a1), centre.y + radius * std::sin(a1)});
    }
}

void appendBand(std::vector<MeshVertex> &mesh, const StrokePoint &a, const StrokePoint &b,
                float halfWidth) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = std::hypot(dx, dy);
    if (length == 0.0F)
        return;
    const float nx = -dy / length * halfWidth;
    const float ny = dx / length * halfWidth;
    const MeshVertex a0{a.x + nx, a.y + ny};
    const MeshVertex a1{a.x - nx, a.y - ny};
    const MeshVertex b0{b.x + nx, b.y + ny};
    const MeshVertex b1{b.x - nx, b.y - ny};
    mesh.insert(mesh.end(), {a0, a1, b0, a1, b1, b0});
}

} // namespace

float pointWidth(const Stroke &stroke, const StrokePoint &point) {
    return std::max(minimumWidth, stroke.width() * point.pressure);
}

std::vector<MeshVertex> buildStrokeMesh(const Stroke &stroke) {
    const std::vector<StrokePoint> &points = stroke.points();
    std::vector<MeshVertex> mesh;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float own = pointWidth(stroke, points[i]);
        const float incoming = i > 0 ? pointWidth(stroke, points[i - 1]) : own;
        appendDisc(mesh, points[i], std::max(own, incoming) / 2.0F);
        if (i + 1 < points.size())
            appendBand(mesh, points[i], points[i + 1], own / 2.0F);
    }
    return mesh;
}

Stroke simplifiedStroke(const Stroke &stroke, float minSpacing) {
    if (!std::isfinite(minSpacing) || minSpacing < 0.0F)
        throw std::invalid_argument("Stroke spacing must be finite and non-negative");
    Stroke result(stroke.argb(), stroke.width(), stroke.kind());
    const std::vector<StrokePoint> &points = stroke.points();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const bool end = i == 0 || i + 1 == points.size();
        const StrokePoint *last = result.points().empty() ? nullptr : &result.points().back();
        if (end || std::hypot(points[i].x - last->x, points[i].y - last->y) >= minSpacing)
            result.addPoint(points[i]);
    }
    return result;
}

} // namespace peerchalk
