#pragma once

#include <QString>

namespace peerchalk::sync {

// The network choices made on the settings page, kept in network.json in the data
// folder. Empty text fields mean "none"; an empty bootstrap means Jami's public node.
struct NetworkSettings {
    bool enabled{true};
    QString bootstrap;  // host:port, comma separated
    QString dhtNetwork; // OpenDHT network id; empty for the public network
    QString stun;       // host:port
    QString turn;       // user:password@host:port

    // The saved settings, or the defaults when none were saved. Throws
    // std::runtime_error when the file is malformed.
    [[nodiscard]] static NetworkSettings load(const QString &directory);
    // Checks the settings (std::invalid_argument naming the field) and saves them,
    // readable only by you as they may hold a TURN password.
    void save(const QString &directory) const;
    // These settings with any PEERCHALK_NETWORK, _BOOTSTRAP, _DHT_NETWORK, _STUN or
    // _TURN environment variable taking precedence.
    [[nodiscard]] NetworkSettings withEnvironment() const;
    // Whether any of those environment variables is set.
    [[nodiscard]] static bool environmentOverrides();

    friend bool operator==(const NetworkSettings &, const NetworkSettings &) = default;
};

} // namespace peerchalk::sync
