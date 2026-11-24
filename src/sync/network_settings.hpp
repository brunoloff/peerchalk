#pragma once

#include <QString>

namespace peerchalk::sync {

// The network choices made on the settings page, kept in network.json in the data
// folder. An empty bootstrap or STUN field means the public defaults; an empty DHT
// network means the public network, an empty TURN field means PeerChalk's relay ("none"
// for no relay) and an empty proxy means Jami's public one.
struct NetworkSettings {
    bool enabled{true};
    QString bootstrap;  // host:port, comma separated
    QString dhtNetwork; // OpenDHT network id; empty for the public network
    QString stun;       // host:port, comma separated
    QString turn;       // user:password@host:port, or "none"
    QString dhtProxy;   // https:// OpenDHT proxies, comma-separated, used by the browser build

    // The saved settings, or the defaults when none were saved. Throws
    // std::runtime_error when the file is malformed.
    [[nodiscard]] static NetworkSettings load(const QString &directory);
    // Checks the settings (std::invalid_argument naming the field) and saves them,
    // readable only by you as they may hold a TURN password.
    void save(const QString &directory) const;
    // These settings with any PEERCHALK_NETWORK, _BOOTSTRAP, _DHT_NETWORK, _STUN,
    // _TURN or _DHT_PROXY environment variable taking precedence.
    [[nodiscard]] NetworkSettings withEnvironment() const;
    // Whether any of those environment variables is set.
    [[nodiscard]] static bool environmentOverrides();

    friend bool operator==(const NetworkSettings &, const NetworkSettings &) = default;
};

} // namespace peerchalk::sync
