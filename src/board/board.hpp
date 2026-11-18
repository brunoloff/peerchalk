#pragma once

#include "board/stroke.hpp"
#include "board/stroke_codec.hpp"
#include "board/text_box.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace peerchalk {

// True when any part of the stroke, including its width, lies within radius of point.
[[nodiscard]] bool strokeNear(const Stroke &stroke, Point point, float radius);

// The strokes on one board, kept in a Yrs document so that peers can merge their
// changes. Strokes live in the document's "strokes" array, one encodeStroke() binary
// per entry, in drawing order; strokes() mirrors that array. Text boxes live beside them
// (see board_text.cpp). Undo reverts only this replica's own changes, never ones applied
// from peers.
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
    // Union of all stroke and text box bounds; empty when the board shows nothing.
    [[nodiscard]] std::optional<Bounds> contentBounds() const;
    [[nodiscard]] bool canUndo() const;

    void addStroke(const Stroke &stroke);
    // Removes every stroke and text box near point and returns how many. Removals
    // until endGesture() form one undo step. Throws std::invalid_argument for
    // non-finite input or a negative radius.
    std::size_t eraseNear(Point point, float radius);
    void endGesture();
    // Reverts this replica's latest step; returns false when there is nothing to undo.
    bool undo();

    // Text boxes by id. These read the document, so they must not be called from the change
    // listener; the pointers and references last until the next change.
    [[nodiscard]] const std::vector<TextBox> &textBoxes() const;
    [[nodiscard]] const TextBox *textBox(const std::string &id) const;
    // Throws std::invalid_argument for an invalid box or an id already in use.
    void addTextBox(const TextBox &box);
    // These change the box with box.id / id, if it still exists. Their changes join the
    // current undo step until endGesture(). Invalid input throws std::invalid_argument.
    void setTextBoxFrame(const TextBox &box); // everything except the body
    void setTextBoxBody(const std::string &id, const RichText &body);
    void removeTextBox(const std::string &id);
    // How tall the interface laid the box out; kept on this device only.
    void setTextBoxHeight(const std::string &id, float height);
    [[nodiscard]] Bounds textBoxBounds(const TextBox &box) const;

    // Synchronisation with peers, using Yrs v1 encodings.
    [[nodiscard]] Bytes stateVector() const;
    // The changes a peer with the given state vector is missing.
    // Throws std::invalid_argument for an empty or malformed state vector.
    [[nodiscard]] Bytes difference(ByteView stateVector) const;
    // Applies a peer's update; returns false when it is empty or malformed.
    bool apply(ByteView update);
    // The whole document as one update, for saving; apply() loads it into a new board.
    [[nodiscard]] Bytes encodeState() const;

    // Called after every change to the board, local or from a peer, while the change is
    // still being committed: the listener must not start another change or read the
    // document through Yrs (stateVector, encodeState, textBoxes...); strokes() is up to date.
    void setChangeListener(std::function<void()> listener) { listener_ = std::move(listener); }

  private:
    struct Native;

    void applyArrayDelta(const void *event);
    void textChanged();
    std::size_t eraseTextBoxesNear(Point point, float radius);

    std::unique_ptr<Native> native_;
    std::vector<Stroke> strokes_;
    mutable std::vector<TextBox> textBoxes_;
    mutable bool textBoxesStale_{true};
    std::map<std::string, float> textBoxHeights_;
    std::function<void()> listener_;
};

} // namespace peerchalk
