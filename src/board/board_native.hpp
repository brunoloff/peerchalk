#pragma once

// The Yrs side of Board, shared by its source files.
#include "board/board.hpp"

extern "C" {
#include <libyrs.h>
}

#include <memory>
#include <stdexcept>

namespace peerchalk {

inline constexpr char boardLocalOrigin[] = "peerchalk-local";

using BoardTransaction = std::unique_ptr<YTransaction, decltype(&ytransaction_commit)>;

struct Board::Native {
    std::unique_ptr<YDoc, decltype(&ydoc_destroy)> doc{nullptr, ydoc_destroy};
    Branch *strokes{nullptr};
    Branch *textBoxes{nullptr};
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
};

} // namespace peerchalk
