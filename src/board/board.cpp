#include "board/board.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace peerchalk {
namespace {

float distanceToSegment(Point p, const StrokePoint &a, const StrokePoint &b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    float t = 0.0F;
    if (lengthSquared > 0.0F)
        t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0F, 1.0F);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

} // namespace

bool strokeNear(const Stroke &stroke, Point point, float radius) {
    const std::optional<Bounds> bounds = stroke.bounds();
    if (!bounds || !bounds->contains(point, radius))
        return false;
    const std::vector<StrokePoint> &points = stroke.points();
    const float reach = radius + stroke.width() / 2.0F;
    if (points.size() == 1)
        return distanceToSegment(point, points.front(), points.front()) <= reach;
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (distanceToSegment(point, points[i - 1], points[i]) <= reach)
            return true;
    }
    return false;
}

std::optional<Bounds> Board::contentBounds() const {
    std::optional<Bounds> result;
    for (const Stroke &stroke : strokes_) {
        if (const std::optional<Bounds> bounds = stroke.bounds())
            result = result ? unite(*result, *bounds) : *bounds;
    }
    return result;
}

void Board::addStroke(Stroke stroke) {
    strokes_.push_back(std::move(stroke));
    history_.push_back({true, {}});
    gestureOpen_ = false;
}

std::size_t Board::eraseNear(Point point, float radius) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(radius) ||
        radius < 0.0F)
        throw std::invalid_argument("Eraser position and radius must be finite and non-negative");
    std::vector<Removal> removed;
    for (std::size_t i = strokes_.size(); i-- > 0;) {
        if (!strokeNear(strokes_[i], point, radius))
            continue;
        removed.push_back({i, std::move(strokes_[i])});
        strokes_.erase(strokes_.begin() + static_cast<std::ptrdiff_t>(i));
    }
    if (removed.empty())
        return 0;
    if (!gestureOpen_ || history_.empty() || history_.back().added)
        history_.push_back({false, {}});
    std::vector<Removal> &step = history_.back().removed;
    step.insert(step.end(), std::make_move_iterator(removed.begin()),
                std::make_move_iterator(removed.end()));
    gestureOpen_ = true;
    return removed.size();
}

bool Board::undo() {
    if (history_.empty())
        return false;
    Step step = std::move(history_.back());
    history_.pop_back();
    gestureOpen_ = false;
    if (step.added) {
        strokes_.pop_back();
        return true;
    }
    for (auto it = step.removed.rbegin(); it != step.removed.rend(); ++it)
        strokes_.insert(strokes_.begin() + static_cast<std::ptrdiff_t>(it->index),
                        std::move(it->stroke));
    return true;
}

} // namespace peerchalk
