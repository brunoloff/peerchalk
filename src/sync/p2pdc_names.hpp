#pragma once

#include <p2pdc/connection.h>
#include <p2pdc/discovery.h>
#include <p2pdc/incoming.h>
#include <p2pdc/node.h>
#include <p2pdc/route.h>

#include <QString>

#include <exception>

namespace peerchalk::sync {

// Readable names of p2pdc's results, for the connection log.
[[nodiscard]] QString describe(p2pdc::ConnectError error);
[[nodiscard]] QString describe(p2pdc::ConnectionState state);
[[nodiscard]] QString describe(p2pdc::WatchError error);
[[nodiscard]] QString describe(p2pdc::AnnouncementError error);
[[nodiscard]] QString describe(p2pdc::IncomingError error);
[[nodiscard]] QString describe(p2pdc::NodeStartStatus status);
[[nodiscard]] QString describe(p2pdc::PeerEvent event);
[[nodiscard]] QString describe(p2pdc::DiscoveryStatus status);
// Such as "direct (host to server-reflexive)" or "TURN relay".
[[nodiscard]] QString describe(const p2pdc::RouteInfo &route);
// The exception's message, or an empty string.
[[nodiscard]] QString describe(const std::exception_ptr &error);
// The first 8 hex digits of a device key: enough to tell devices apart, not to impersonate one.
[[nodiscard]] QString shortDevice(const p2pdc::SigningPublicKey &key);

} // namespace peerchalk::sync
