#include "sync/board_diagnosis.hpp"

namespace peerchalk::sync {

// The first that applies: being connected outweighs every server problem, and a connection
// in progress outweighs the servers' state, since it shows that discovery worked.
BoardConnectionState diagnose(const BoardConnectionFacts &facts) {
    using enum BoardConnectionState;
    if (!facts.online)
        return Offline;
    if (facts.direct > 0)
        return Connected;
    if (facts.relayed > 0)
        return Relayed;
    if (facts.updateNeeded)
        return UpdateNeeded;
    if (facts.connecting > 0)
        return facts.longestConnectingMs >= stuckAfterMs ? Stuck : Connecting;
    if (facts.discoveryWorking == 0)
        return facts.discoveryChecking == 0 && facts.discoveryUnreachable > 0 ? NoDiscovery
                                                                              : Checking;
    if (!facts.announced)
        return Announcing;
    return Alone;
}

QString stateName(BoardConnectionState state) {
    using enum BoardConnectionState;
    switch (state) {
    case Offline:
        return QStringLiteral("offline");
    case Connected:
        return QStringLiteral("connected");
    case Relayed:
        return QStringLiteral("relayed");
    case UpdateNeeded:
        return QStringLiteral("updateNeeded");
    case Stuck:
        return QStringLiteral("stuck");
    case Connecting:
        return QStringLiteral("connecting");
    case NoDiscovery:
        return QStringLiteral("noDiscovery");
    case Checking:
        return QStringLiteral("checking");
    case Announcing:
        return QStringLiteral("announcing");
    case Alone:
        return QStringLiteral("alone");
    }
    return QStringLiteral("offline");
}

} // namespace peerchalk::sync
