#pragma once

#include "board/stroke.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace peerchalk {

// True when any part of the stroke, including its width, lies within radius of point.
[[nodiscard]] bool strokeNear(const Stroke &stroke, Point point, float radius);

// The strokes on one board, in drawing order, with an undo history.
class Board {
  public:
    [[nodiscard]] const std::vector<Stroke> &strokes() const { return strokes_; }
    // Union of all stroke bounds; empty when the board has no strokes with points.
    [[nodiscard]] std::optional<Bounds> contentBounds() const;
    [[nodiscard]] bool canUndo() const { return !history_.empty(); }

    void addStroke(Stroke stroke);
    // Removes every stroke near point and returns how many were removed. Removals
    // until endGesture() form one undo step. Throws std::invalid_argument for
    // non-finite input or a negative radius.
    std::size_t eraseNear(Point point, float radius);
    void endGesture() { gestureOpen_ = false; }
    // Reverts the latest step; returns false when there is nothing to undo.
    bool undo();

  private:
    struct Removal {
        std::size_t index;
        Stroke stroke;
    };
    struct Step {
        bool added;                   // true: one stroke appended; false: removals.
        std::vector<Removal> removed; // in removal order
    };

    std::vector<Stroke> strokes_;
    std::vector<Step> history_;
    bool gestureOpen_{false};
};

} // namespace peerchalk
