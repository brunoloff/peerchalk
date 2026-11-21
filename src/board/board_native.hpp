#pragma once

// The Yrs side of Board, shared by its source files.
#include "board/board.hpp"

extern "C" {
#include <libyrs.h>
}

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace peerchalk {

inline constexpr char boardLocalOrigin[] = "peerchalk-local";

using BoardTransaction = std::unique_ptr<YTransaction, decltype(&ytransaction_commit)>;

using Output = std::unique_ptr<YOutput, decltype(&youtput_destroy)>;

inline std::optional<double> readNumber(const YOutput *value) {
    if (value == nullptr)
        return std::nullopt;
    if (value->tag == Y_JSON_NUM)
        return *youtput_read_float(value);
    if (value->tag == Y_JSON_INT)
        return static_cast<double>(*youtput_read_long(value));
    return std::nullopt;
}

// A finite number stored under key, if there is one.
inline std::optional<double> mapNumber(const Branch *map, const YTransaction *txn,
                                       const char *key) {
    const Output value(ymap_get(map, txn, key), youtput_destroy);
    const std::optional<double> number = readNumber(value.get());
    return number && std::isfinite(*number) ? number : std::nullopt;
}

// The string stored under key, or "" when there is none.
inline std::string mapString(const Branch *map, const YTransaction *txn, const char *key) {
    const Output value(ymap_get(map, txn, key), youtput_destroy);
    return value && value->tag == Y_JSON_STR ? std::string(youtput_read_string(value.get()))
                                             : std::string();
}

// The length of a binary payload for Yrs; throws std::length_error beyond 4 GiB.
[[nodiscard]] std::uint32_t checkedLength(ByteView bytes);
// A stroke entry; one that is not a valid stroke becomes a stroke with no points, so mirrors
// stay index-aligned with their Yrs arrays.
[[nodiscard]] Stroke decodeStrokeEntry(const YOutput &value);

struct Board::Native {
    std::unique_ptr<YDoc, decltype(&ydoc_destroy)> doc{nullptr, ydoc_destroy};
    Branch *strokes{nullptr};
    Branch *textBoxes{nullptr};
    Branch *layout{nullptr};      // page boards: {kind}
    Branch *columns{nullptr};     // page boards: the columns and their items
    Branch *pageStrokes{nullptr}; // page boards: page id -> strokes in page coordinates
    std::unique_ptr<YUndoManager, decltype(&yundo_manager_destroy)> undo{nullptr,
                                                                         yundo_manager_destroy};

    // Local changes carry an origin so that the undo manager tracks only them.
    [[nodiscard]] BoardTransaction write(bool local) const {
        BoardTransaction txn(local ? ydoc_write_transaction(doc.get(), sizeof(boardLocalOrigin) - 1,
                                                            boardLocalOrigin)
                                   : ydoc_write_transaction(doc.get(), 0, nullptr),
                             ytransaction_commit);
        if (!txn)
            throw std::runtime_error("Yrs board transaction could not start");
        return txn;
    }

    [[nodiscard]] BoardTransaction read() const {
        BoardTransaction txn(ydoc_read_transaction(doc.get()), ytransaction_commit);
        if (!txn)
            throw std::runtime_error("Yrs board transaction could not start");
        return txn;
    }
};

} // namespace peerchalk
