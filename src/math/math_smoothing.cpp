#include "math/math_smoothing.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace peerchalk {
namespace {

constexpr float maxMiter = 2.0F;
constexpr float maxBandEm = 0.05F;

struct Vec {
    float x{0.0F};
    float y{0.0F};
};

// A corner of the shape, shared by every triangle that touches its position.
struct Corner {
    Vec position;
    Vec normalSum;
    float minAlignment{1.0F}; // how far the outline turns here
    bool onEdge{false};
};

struct Edge {
    quint32 a;
    quint32 b;
    quint32 opposite;
    int uses;
};

quint64 positionKey(float x, float y) {
    return quint64(std::bit_cast<quint32>(x + 0.0F)) << 32 | std::bit_cast<quint32>(y + 0.0F);
}

quint64 edgeKey(quint32 a, quint32 b) {
    return quint64(std::min(a, b)) << 32 | std::max(a, b);
}

// The run's triangles over corners with unique positions.
struct Welded {
    std::vector<Corner> corners;
    std::vector<quint32> indices;
};

Welded weldRun(const MathFormula &formula, const MathRun &run) {
    Welded out;
    std::unordered_map<quint64, quint32> byPosition;
    std::unordered_map<quint32, quint32> bySource;
    out.indices.reserve(run.indexCount);
    for (quint32 i = run.firstIndex; i < run.firstIndex + run.indexCount; ++i) {
        const quint32 source = formula.indices[i];
        const auto known = bySource.find(source);
        if (known != bySource.end()) {
            out.indices.push_back(known->second);
            continue;
        }
        const Vec at{formula.vertices[source * 2], formula.vertices[source * 2 + 1]};
        const auto [slot, added] =
            byPosition.try_emplace(positionKey(at.x, at.y), quint32(out.corners.size()));
        if (added)
            out.corners.push_back({at, {}, 1.0F, false});
        bySource.emplace(source, slot->second);
        out.indices.push_back(slot->second);
    }
    return out;
}

std::vector<Edge> outlineEdges(const std::vector<quint32> &indices) {
    std::unordered_map<quint64, Edge> edges;
    edges.reserve(indices.size());
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        for (std::size_t k = 0; k < 3; ++k) {
            const quint32 a = indices[i + k];
            const quint32 b = indices[i + (k + 1) % 3];
            if (a == b)
                continue;
            auto [slot, added] = edges.try_emplace(edgeKey(a, b), Edge{a, b, indices[i + (k + 2) % 3], 0});
            ++slot->second.uses;
        }
    }
    std::vector<Edge> outline;
    for (const auto &[key, edge] : edges) {
        if (edge.uses == 1)
            outline.push_back(edge);
    }
    return outline;
}

// The unit normal of the edge, pointing away from the triangle it belongs to.
Vec outwardNormal(const std::vector<Corner> &corners, const Edge &edge) {
    const Vec a = corners[edge.a].position;
    const Vec b = corners[edge.b].position;
    const Vec inner = corners[edge.opposite].position;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = std::hypot(dx, dy);
    if (length == 0.0F)
        return {};
    Vec normal{dy / length, -dx / length};
    if (normal.x * (inner.x - a.x) + normal.y * (inner.y - a.y) > 0.0F)
        normal = {-normal.x, -normal.y};
    return normal;
}

void collectNormals(std::vector<Corner> &corners, const std::vector<Edge> &outline) {
    std::vector<Vec> normals;
    normals.reserve(outline.size());
    for (const Edge &edge : outline) {
        const Vec normal = outwardNormal(corners, edge);
        normals.push_back(normal);
        for (const quint32 end : {edge.a, edge.b}) {
            corners[end].normalSum.x += normal.x;
            corners[end].normalSum.y += normal.y;
            corners[end].onEdge = true;
        }
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
        for (const quint32 end : {outline[i].a, outline[i].b}) {
            Corner &corner = corners[end];
            const float length = std::hypot(corner.normalSum.x, corner.normalSum.y);
            if (length == 0.0F)
                continue;
            const float alignment =
                (corner.normalSum.x * normals[i].x + corner.normalSum.y * normals[i].y) / length;
            corner.minAlignment = std::min(corner.minAlignment, alignment);
        }
    }
}

// How far, and in which direction, half a band moves this corner.
Vec cornerOffset(const Corner &corner, float halfBand) {
    const float length = std::hypot(corner.normalSum.x, corner.normalSum.y);
    if (!corner.onEdge || length == 0.0F)
        return {};
    const float miter = std::min(maxMiter, 1.0F / std::max(corner.minAlignment, 1.0F / maxMiter));
    const float reach = halfBand * miter / length;
    return {corner.normalSum.x * reach, corner.normalSum.y * reach};
}

} // namespace

SmoothMesh smoothMathRun(const MathFormula &formula, const MathRun &run, float pixelEm) {
    if (!(pixelEm > 0.0F) || !std::isfinite(pixelEm))
        throw std::invalid_argument("smoothMathRun needs a positive pixel size");
    Welded welded = weldRun(formula, run);
    const std::vector<Edge> outline = outlineEdges(welded.indices);
    collectNormals(welded.corners, outline);
    const float halfBand = std::min(pixelEm, maxBandEm) / 2.0F;
    SmoothMesh mesh;
    const auto count = quint32(welded.corners.size());
    mesh.vertices.reserve(std::size_t(count) * 2);
    std::vector<quint32> outer(count, 0);
    for (const Corner &corner : welded.corners) {
        const Vec offset = cornerOffset(corner, halfBand);
        mesh.vertices.push_back({corner.position.x - offset.x, corner.position.y - offset.y, 1.0F});
    }
    for (quint32 i = 0; i < count; ++i) {
        if (!welded.corners[i].onEdge)
            continue;
        const Vec offset = cornerOffset(welded.corners[i], halfBand);
        outer[i] = quint32(mesh.vertices.size());
        mesh.vertices.push_back(
            {welded.corners[i].position.x + offset.x, welded.corners[i].position.y + offset.y, 0.0F});
    }
    mesh.indices = std::move(welded.indices);
    mesh.indices.reserve(mesh.indices.size() + outline.size() * 6);
    for (const Edge &edge : outline) {
        const quint32 quad[] = {edge.a, edge.b, outer[edge.b], edge.a, outer[edge.b], outer[edge.a]};
        mesh.indices.insert(mesh.indices.end(), std::begin(quad), std::end(quad));
    }
    return mesh;
}

} // namespace peerchalk
