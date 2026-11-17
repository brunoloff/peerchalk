#include "board/board.hpp"

extern "C" {
#include <libyrs.h>
}

#include <algorithm>
#include <climits>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr char strokesName[] = "strokes";
constexpr char observerKey[] = "peerchalk-board";
constexpr char localOrigin[] = "peerchalk-local";

using Transaction = std::unique_ptr<YTransaction, decltype(&ytransaction_commit)>;

std::uint32_t checkedLength(ByteView bytes) {
    if (bytes.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Yrs payload is too large");
    return static_cast<std::uint32_t>(bytes.size());
}

Bytes takeBinary(char *data, std::uint32_t length) {
    if (data == nullptr)
        throw std::runtime_error("Yrs could not encode the board");
    auto release = [length](char *value) { ybinary_destroy(value, length); };
    std::unique_ptr<char, decltype(release)> owned(data, release);
    const auto *begin = reinterpret_cast<const std::byte *>(data);
    return {begin, begin + length};
}

float distanceToSegment(Point p, const StrokePoint &a, const StrokePoint &b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    float t = 0.0F;
    if (lengthSquared > 0.0F)
        t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0F, 1.0F);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

// A peer's entry that is not a valid stroke is kept as an empty stroke, so the mirror
// stays index-aligned with the Yrs array.
Stroke decodeEntry(const YOutput &value) {
    if (value.tag == Y_JSON_BUF) {
        const auto *data = reinterpret_cast<const std::byte *>(youtput_read_binary(&value));
        if (std::optional<Stroke> stroke = decodeStroke(ByteView(data, value.len)))
            return *std::move(stroke);
    }
    return Stroke(0U, 1.0F);
}

} // namespace

struct Board::Native {
    std::unique_ptr<YDoc, decltype(&ydoc_destroy)> doc{ydoc_new(), ydoc_destroy};
    Branch *strokes{nullptr};
    std::unique_ptr<YUndoManager, decltype(&yundo_manager_destroy)> undo{nullptr,
                                                                         yundo_manager_destroy};

    // Local changes carry an origin so that the undo manager tracks only them.
    [[nodiscard]] Transaction write(bool local) const {
        Transaction txn(
            local ? ydoc_write_transaction(doc.get(), sizeof(localOrigin) - 1, localOrigin)
                  : ydoc_write_transaction(doc.get(), 0, nullptr),
            ytransaction_commit);
        if (!txn)
            throw std::runtime_error("Yrs board transaction could not start");
        return txn;
    }
};

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

Board::Board() : native_(std::make_unique<Native>()) {
    if (!native_->doc || !(native_->strokes = yarray(native_->doc.get(), strokesName)))
        throw std::runtime_error("Yrs board document could not be created");
    const YUndoManagerOptions options{INT_MAX}; // steps end only at yundo_manager_stop
    native_->undo.reset(yundo_manager(&options));
    if (!native_->undo)
        throw std::runtime_error("Yrs undo manager could not be created");
    yundo_manager_add_scope(native_->undo.get(), native_->doc.get(), native_->strokes);
    yundo_manager_add_origin(native_->undo.get(), sizeof(localOrigin) - 1, localOrigin);
    yarray_observe(native_->strokes, sizeof(observerKey) - 1, observerKey, this,
                   [](void *state, const YArrayEvent *event) {
                       // Exceptions must not unwind through the Rust library.
                       try {
                           static_cast<Board *>(state)->applyArrayDelta(event);
                       } catch (...) {
                           std::terminate();
                       }
                   });
}

Board::~Board() {
    yunobserve(native_->strokes, sizeof(observerKey) - 1, observerKey);
    native_->undo.reset();
}

void Board::applyArrayDelta(const void *event) {
    std::uint32_t length = 0;
    YEventChange *delta = yarray_event_delta(static_cast<const YArrayEvent *>(event), &length);
    auto release = [length](YEventChange *changes) { yevent_delta_destroy(changes, length); };
    std::unique_ptr<YEventChange, decltype(release)> owned(delta, release);
    auto index = static_cast<std::ptrdiff_t>(0);
    for (std::uint32_t i = 0; i < length; ++i) {
        const YEventChange &change = delta[i];
        const auto count = static_cast<std::ptrdiff_t>(change.len);
        if (change.tag == Y_EVENT_CHANGE_RETAIN) {
            index += count;
        } else if (change.tag == Y_EVENT_CHANGE_DELETE) {
            strokes_.erase(strokes_.begin() + index, strokes_.begin() + index + count);
        } else if (change.tag == Y_EVENT_CHANGE_ADD) {
            for (std::uint32_t k = 0; k < change.len; ++k)
                strokes_.insert(strokes_.begin() + index++, decodeEntry(change.values[k]));
        }
    }
}

std::optional<Bounds> Board::contentBounds() const {
    std::optional<Bounds> result;
    for (const Stroke &stroke : strokes_) {
        if (const std::optional<Bounds> bounds = stroke.bounds())
            result = result ? unite(*result, *bounds) : *bounds;
    }
    return result;
}

bool Board::canUndo() const { return yundo_manager_undo_stack_len(native_->undo.get()) > 0; }

void Board::addStroke(const Stroke &stroke) {
    const Bytes encoded = encodeStroke(stroke);
    {
        Transaction txn = native_->write(true);
        const YInput input =
            yinput_binary(reinterpret_cast<const char *>(encoded.data()), checkedLength(encoded));
        yarray_insert_range(native_->strokes, txn.get(), yarray_len(native_->strokes), &input, 1);
    }
    yundo_manager_stop(native_->undo.get());
}

std::size_t Board::eraseNear(Point point, float radius) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(radius) ||
        radius < 0.0F)
        throw std::invalid_argument("Eraser position and radius must be finite and non-negative");
    std::vector<std::uint32_t> hits;
    for (std::size_t i = strokes_.size(); i-- > 0;) {
        if (strokeNear(strokes_[i], point, radius))
            hits.push_back(static_cast<std::uint32_t>(i));
    }
    if (hits.empty())
        return 0;
    Transaction txn = native_->write(true);
    for (const std::uint32_t index : hits) // descending, so earlier indices stay valid
        yarray_remove_range(native_->strokes, txn.get(), index, 1);
    return hits.size();
}

void Board::endGesture() { yundo_manager_stop(native_->undo.get()); }

bool Board::undo() { return canUndo() && yundo_manager_undo(native_->undo.get()) == Y_TRUE; }

Bytes Board::stateVector() const {
    Transaction txn = native_->write(false);
    std::uint32_t length = 0;
    char *data = ytransaction_state_vector_v1(txn.get(), &length);
    return takeBinary(data, length);
}

Bytes Board::difference(ByteView stateVector) const {
    if (stateVector.empty())
        throw std::invalid_argument("Yrs state vector is empty");
    Transaction txn = native_->write(false);
    std::uint32_t length = 0;
    char *data =
        ytransaction_state_diff_v1(txn.get(), reinterpret_cast<const char *>(stateVector.data()),
                                   checkedLength(stateVector), &length);
    if (data == nullptr)
        throw std::invalid_argument("Yrs state vector is malformed");
    return takeBinary(data, length);
}

Bytes Board::encodeState() const {
    // A state vector with no entries: the difference is the whole document.
    const std::byte emptyStateVector[] = {std::byte{0}};
    return difference(emptyStateVector);
}

bool Board::apply(ByteView update) {
    if (update.empty())
        return false;
    Transaction txn = native_->write(false);
    return ytransaction_apply(txn.get(), reinterpret_cast<const char *>(update.data()),
                              checkedLength(update)) == 0;
}

} // namespace peerchalk
