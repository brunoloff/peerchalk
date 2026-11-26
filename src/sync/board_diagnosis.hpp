#pragma once

#include <QString>
#include <QtGlobal>

namespace peerchalk::sync {

// What this device knows about reaching the others on one board, for the board's network
// pane. Network gathers it; diagnose turns it into the one state the pane explains.
struct BoardConnectionFacts {
    bool online{false};       // the node is running
    bool updateNeeded{false}; // someone here runs a version too new to talk to
    // Discovery servers by what the last checks found (the desktop DHT counts as working
    // while online).
    int discoveryWorking{0};
    int discoveryUnreachable{0};
    int discoveryChecking{0};
    bool relayConfigured{false}; // a TURN relay is set
    bool relayWorking{false};    // and answered its last check
    bool announced{false};       // this device's advertisement is out in the board's spaces
    int seen{0};                 // other devices advertising there
    int connecting{0};           // connections not ready yet
    qint64 longestConnectingMs{0};
    int direct{0};  // ready connections, direct
    int relayed{0}; // and through a relay
};

enum class BoardConnectionState {
    Offline,      // networking is off or has not started
    Connected,    // at least one direct connection
    Relayed,      // connected, but only through a relay
    UpdateNeeded, // nobody connected, and someone runs a newer version
    Stuck,        // devices found, but no connection after stuckAfterMs
    Connecting,   // devices found, connecting
    NoDiscovery,  // no discovery server answers
    Checking,     // discovery servers not checked yet
    Announcing,   // servers answer; this device is not announced yet
    Alone,        // announced, and nobody else is here
};

// How long a connection may take before the pane says it is stuck.
inline constexpr qint64 stuckAfterMs = 20000;

[[nodiscard]] BoardConnectionState diagnose(const BoardConnectionFacts &facts);
// "offline", "connected", "relayed", "updateNeeded", "stuck", "connecting", "noDiscovery",
// "checking", "announcing" or "alone", as the pane's QML knows them.
[[nodiscard]] QString stateName(BoardConnectionState state);

} // namespace peerchalk::sync
