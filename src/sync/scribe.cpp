#include "sync/scribe.hpp"

namespace peerchalk::sync {

std::optional<QByteArray> electScribe(const std::vector<ScribeCandidate> &devices) {
    std::optional<QByteArray> scribe;
    for (const ScribeCandidate &device : devices)
        if (device.member && device.canPush && (!scribe || device.device < *scribe))
            scribe = device.device;
    return scribe;
}

} // namespace peerchalk::sync
