#pragma once

#include "board/stroke.hpp"
#include "board/stroke_codec.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace peerchalk {

// True when any part of the stroke, including its width, lies within radius of point.
[[nodiscard]] bool strokeNear(const Stroke &stroke, Point point, float radius);

// The strokes on one board, kept in a Yrs document so that peers can merge their
// changes. Strokes live in the document's "strokes" array, one encodeStroke() binary
// per entry, in drawing order; strokes() mirrors that array. Undo reverts only this
// replica's own changes, never ones applied from peers.
class Board {
  public:
    Board();
    ~Board();
    Board(const Board &) = delete;
    Board &operator=(const Board &) = delete;
    Board(Board &&) = delete;
    Board &operator=(Board &&) = delete;

    // Entries a peer sent that could not be decoded appear as strokes with no points.
    [[nodiscard]] const std::vector<Stroke> &strokes() const { return strokes_; }
    // Union of all stroke bounds; empty when the board has no strokes with points.
    [[nodiscard]] std::optional<Bounds> contentBounds() const;
    [[nodiscard]] bool canUndo() const;

    void addStroke(const Stroke &stroke);
    // Removes every stroke near point and returns how many were removed. Removals
    // until endGesture() form one undo step. Throws std::invalid_argument for
    // non-finite input or a negative radius.
    std::size_t eraseNear(Point point, float radius);
    void endGesture();
    // Reverts this replica's latest step; returns false when there is nothing to undo.
    bool undo();

    // Synchronisation with peers, using Yrs v1 encodings.
    [[nodiscard]] Bytes stateVector() const;
    // The changes a peer with the given state vector is missing.
    // Throws std::invalid_argument for an empty or malformed state vector.
    [[nodiscard]] Bytes difference(ByteView stateVector) const;
    // Applies a peer's update; returns false when it is empty or malformed.
    bool apply(ByteView update);
    // The whole document as one update, for saving; apply() loads it into a new board.
    [[nodiscard]] Bytes encodeState() const;

    // Called after every change to the strokes, local or from a peer, while the change
    // is still being committed: the listener must not start another change or read the
    // document through Yrs (stateVector, encodeState...); strokes() is up to date.
    void setChangeListener(std::function<void()> listener) { listener_ = std::move(listener); }

  private:
    struct Native;

    void applyArrayDelta(const void *event);

    std::unique_ptr<Native> native_;
    std::vector<Stroke> strokes_;
    std::function<void()> listener_;
};

} // namespace peerchalk
