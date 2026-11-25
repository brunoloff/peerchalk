#pragma once

#include "hosted/hosted_messages.hpp"
#include "repository/board_loader.hpp"

#include <QString>

namespace peerchalk::hosted {

// Turns an "open" into an account folder the app can sign in to: the identity and device
// the dashboard gave, the user's name, and a workspace holding only the board, marked as a
// repository board, with its access log and merged content. Throws MessageError when the
// board's files are unusable or this user is not a member, std::runtime_error when the
// folder cannot be written.
[[nodiscard]] repository::LoadedBoard prepareAccount(const QString &directory,
                                                     const OpenMessage &open);

} // namespace peerchalk::hosted
