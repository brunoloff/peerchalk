#pragma once

#include "board/stroke.hpp"

#include <vector>

namespace peerchalk {

struct MeshVertex {
    float x;
    float y;
};

// Converts a stroke into a triangle strip whose half-width at each point is
// width * pressure / 2, so wide lines render on every graphics backend.
// Strokes with fewer than two distinct points produce a small square dot;
// strokes with no points produce an empty strip.
[[nodiscard]] std::vector<MeshVertex> buildTriangleStrip(const Stroke &stroke);

} // namespace peerchalk
