#pragma once

#include "sync/network_settings.hpp"

#include <p2pdc/discovery_config.h>
#include <p2pdc/ice_config.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace peerchalk::sync {

// Where PeerChalk looks for peers and how it reaches them.
struct NetworkConfig {
    bool enabled{true};
    std::vector<p2pdc::BootstrapServer> bootstrap;
    std::uint32_t networkId{0}; // 0: the public OpenDHT network
    bool loopbackOnly{false};   // bind to 127.0.0.1 and ::1 only (local tests)
    p2pdc::IceConfig ice;
    std::vector<p2pdc::DiscoveryBackend> discoveryBackends; // where nodes meet besides the DHT
    // How often another waiting project or board gets a turn when there are too many.
    std::chrono::milliseconds spaceRotation{std::chrono::minutes(2)};
    // Whether this device meets the user's other devices. Off in PeerChalk hosted by a
    // dashboard, which holds one repository board and no workspace of its own.
    bool personalSpace{true};

    // The configuration the settings describe, defaulting to the public OpenDHT network
    // through Jami's bootstrap node and proxy (browsers use the proxies, desktops the nodes),
    // then PeerChalk's own (deploy/dht-proxy/), and also two Nostr relays, HiveMQ's MQTT broker
    // and the public PeerJS server;
    // Cloudflare's and Google's public STUN servers and PeerChalk's relay as TURN server ("none"
    // for no TURN server). Throws std::invalid_argument naming the setting when a value is
    // malformed.
    [[nodiscard]] static NetworkConfig from(const NetworkSettings &settings);
};

} // namespace peerchalk::sync
