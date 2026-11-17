#pragma once

#include <algorithm>

namespace peerchalk {

struct Point {
    float x;
    float y;
};

// An axis-aligned rectangle; right >= left and bottom >= top.
struct Bounds {
    float left;
    float top;
    float right;
    float bottom;

    [[nodiscard]] float width() const { return right - left; }
    [[nodiscard]] float height() const { return bottom - top; }
    [[nodiscard]] bool contains(Point point, float margin = 0.0F) const {
        return point.x >= left - margin && point.x <= right + margin && point.y >= top - margin &&
               point.y <= bottom + margin;
    }
};

[[nodiscard]] inline Bounds unite(const Bounds &a, const Bounds &b) {
    return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
            std::max(a.bottom, b.bottom)};
}

} // namespace peerchalk
