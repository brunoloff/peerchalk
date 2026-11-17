#pragma once

#include <p2pdc/discovery_config.h>
#include <p2pdc/ice_config.h>

#include <QString>

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

    // From the environment, defaulting to the public OpenDHT network through Jami's
    // bootstrap node and no STUN or TURN servers:
    //   PEERCHALK_NETWORK=off            disables networking
    //   PEERCHALK_BOOTSTRAP=host:port,…  OpenDHT bootstrap nodes
    //   PEERCHALK_DHT_NETWORK=<number>   OpenDHT network id
    //   PEERCHALK_STUN=host:port         a STUN server
    //   PEERCHALK_TURN=user:password@host:port   a TURN server (UDP)
    // Throws std::invalid_argument naming the variable when a value is malformed.
    static NetworkConfig fromEnvironment();
};

} // namespace peerchalk::sync
