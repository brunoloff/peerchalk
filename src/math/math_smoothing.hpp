#pragma once

#include "math/math_engine.hpp"

#include <vector>

namespace peerchalk {

struct SmoothVertex {
    float x;
    float y;
    float alpha;
};

struct SmoothMesh {
    std::vector<SmoothVertex> vertices;
    std::vector<quint32> indices;
};

// One run's triangles with soft edges, so that they look smooth without multisampling: the
// outline moves in by half a pixel and a one-pixel band fades out across the original edge.
// pixelEm is the size of a screen pixel in em and must be positive.
[[nodiscard]] SmoothMesh smoothMathRun(const MathFormula &formula, const MathRun &run,
                                       float pixelEm);

} // namespace peerchalk
