#pragma once

#include "board/stroke.hpp"

namespace peerchalk {

// Builds a stroke from raw pointer samples in world coordinates, filtering them the way
// pressure-sensitive drawing needs:
// - Pressure is raised to at least minimumPressure, so light touches still draw.
// - With a pressure sensor, samples reporting zero pressure after the first are dropped;
//   pens send them when lifting off. Without a sensor, and for the highlighter, every
//   point gets pressure 1 (constant width).
// - Samples closer than minimumSpacing to the last point are dropped, except that a
//   one-point stroke (a dot) keeps the highest pressure seen while it is held.
// - Where the width changes by more than maximumWidthStep between samples, evenly
//   spaced intermediate points are inserted so the outline changes gradually.
class StrokeBuilder {
  public:
    static constexpr float minimumPressure = 0.05F;
    static constexpr float minimumSpacing = 0.4F;
    static constexpr float maximumWidthStep = 0.3F;

    // Throws std::invalid_argument as Stroke does for an invalid width.
    StrokeBuilder(std::uint32_t argb, float width, StrokeKind kind, bool pressureSensitive);

    // Adds one sample; rawPressure is ignored without a sensor. Throws
    // std::invalid_argument for non-finite input.
    void add(Point world, float rawPressure);

    [[nodiscard]] const Stroke &stroke() const { return stroke_; }
    [[nodiscard]] bool empty() const { return stroke_.points().empty(); }

  private:
    [[nodiscard]] float filteredPressure(float raw) const;
    void addInterpolated(const StrokePoint &from, const StrokePoint &to);

    Stroke stroke_;
    bool usesPressure_;
};

} // namespace peerchalk
