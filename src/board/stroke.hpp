#pragma once

#include "board/geometry.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace peerchalk {

struct StrokePoint {
    float x;
    float y;
    float pressure; // 0 to 1; 1 when the input device has no pressure sensor.
};

// A freehand pen stroke: a polyline with per-point pressure, one colour and a base width.
class Stroke {
  public:
    // Throws std::invalid_argument unless width is finite and positive.
    Stroke(std::uint32_t argb, float width);

    // Throws std::invalid_argument for non-finite coordinates or pressure outside [0, 1].
    void addPoint(StrokePoint point);

    [[nodiscard]] std::uint32_t argb() const { return argb_; }
    [[nodiscard]] float width() const { return width_; }
    [[nodiscard]] const std::vector<StrokePoint> &points() const { return points_; }
    // Bounds of the point centres widened by half the stroke width; empty for no points.
    [[nodiscard]] std::optional<Bounds> bounds() const;

  private:
    std::uint32_t argb_;
    float width_;
    std::vector<StrokePoint> points_;
};

} // namespace peerchalk
