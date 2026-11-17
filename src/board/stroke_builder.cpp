#include "board/stroke_builder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace peerchalk {

StrokeBuilder::StrokeBuilder(std::uint32_t argb, float width, StrokeKind kind,
                             bool pressureSensitive)
    : stroke_(argb, width, kind),
      usesPressure_(pressureSensitive && kind != StrokeKind::Highlighter) {}

float StrokeBuilder::filteredPressure(float raw) const {
    if (!usesPressure_)
        return 1.0F;
    return std::clamp(raw, minimumPressure, 1.0F);
}

void StrokeBuilder::add(Point world, float rawPressure) {
    if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(rawPressure))
        throw std::invalid_argument("Stroke samples must be finite");
    if (usesPressure_ && rawPressure <= 0.0F && !empty())
        return; // lift-off artefact
    const StrokePoint next{world.x, world.y, filteredPressure(rawPressure)};
    if (empty()) {
        stroke_.addPoint(next);
        return;
    }
    const StrokePoint last = stroke_.points().back();
    if (std::hypot(next.x - last.x, next.y - last.y) < minimumSpacing) {
        if (stroke_.points().size() == 1 && next.pressure > last.pressure)
            stroke_.replaceLastPoint({last.x, last.y, next.pressure});
        return;
    }
    addInterpolated(last, next);
    stroke_.addPoint(next);
}

void StrokeBuilder::addInterpolated(const StrokePoint &from, const StrokePoint &to) {
    const float widthChange = std::abs(to.pressure - from.pressure) * stroke_.width();
    const float distance = std::hypot(to.x - from.x, to.y - from.y);
    const int steps = std::min(static_cast<int>(std::ceil(widthChange / maximumWidthStep)),
                               static_cast<int>(std::floor(distance / minimumSpacing)));
    for (int step = 1; step < steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        stroke_.addPoint({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                          from.pressure + (to.pressure - from.pressure) * t});
    }
}

} // namespace peerchalk
