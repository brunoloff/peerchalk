#pragma once

#include "board/stroke.hpp"

#include <vector>

namespace peerchalk {

struct MeshVertex {
    float x;
    float y;
};

// The width a stroke is drawn with at one of its points: base width times pressure,
// never thinner than a hairline so light touches stay visible.
[[nodiscard]] float pointWidth(const Stroke &stroke, const StrokePoint &point);

// Converts a stroke into triangles (three vertices each) that cover its outline:
// - each segment is a band of constant width, the width at its first point;
// - each point gets a round disc as wide as the wider band touching it, which rounds
//   the caps, smooths the joints and hides width steps between bands;
// - a one-point stroke is a round dot.
// The pieces overlap, so the mesh suits opaque colours or drawing into an opaque layer.
[[nodiscard]] std::vector<MeshVertex> buildStrokeMesh(const Stroke &stroke);

} // namespace peerchalk
