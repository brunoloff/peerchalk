#pragma once

#include "sync/network_settings.hpp"

#include <p2pdc/discovery_config.h>
#include <p2pdc/ice_config.h>

#include <cstdint>
#include <vector>

namespace peerchalk::sync {

// Where PeerChalk looks for peers and how it reaches them.
struct NetworkConfig {
    bool enabled{true};
    std::vector<p2pdc::BootstrapServer> bootstrap;
    std::uint32_t networkId{0}; // 0: the public OpenDHT network
    bool loopbackOnly{false};   // bind to 127.0.0.1 and ::1 only (local tests)
    p2pdc::IceConfig ice;

    // The configuration the settings describe, defaulting to the public OpenDHT network
    // through Jami's bootstrap node, Cloudflare's and Google's public STUN servers and no
    // TURN server. Throws
    // std::invalid_argument naming the setting when a value is malformed.
    [[nodiscard]] static NetworkConfig from(const NetworkSettings &settings);
};

} // namespace peerchalk::sync
