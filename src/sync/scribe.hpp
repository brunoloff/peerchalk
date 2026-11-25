#pragma once

#include <QByteArray>

#include <optional>
#include <vector>

namespace peerchalk::sync {

// A device in a repository board's live session, as far as choosing its scribe goes.
struct ScribeCandidate {
    QByteArray device;   // device key
    bool member{false};  // its user is a member of the board or above
    bool canPush{false}; // its dashboard can push the board's branch
};

// The scribe: the member device that can push with the lowest key (bytewise), if any. Every
// device computes this from what it knows, so there is no vote.
[[nodiscard]] std::optional<QByteArray> electScribe(const std::vector<ScribeCandidate> &devices);

} // namespace peerchalk::sync
