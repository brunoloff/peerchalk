#pragma once

#include <QDateTime>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace peerchalk::sync {

// A project, shared board or personal space this device could look for peers in.
struct SpaceCandidate {
    QString key;
    bool essential{false}; // your own devices, or what is open now
    bool connected{false}; // peers are connected through it
    QDateTime lastEdited;
};

// p2pdc can watch only so many spaces, so when there are more than `capacity` the ones
// that matter stay open and `rotatingSlots` of them take turns among the rest, moving
// on by one each `step`. Returns the keys to keep open.
[[nodiscard]] std::vector<QString> chooseSpaces(std::vector<SpaceCandidate> candidates,
                                                std::size_t capacity, std::size_t rotatingSlots,
                                                std::uint64_t step);

} // namespace peerchalk::sync
