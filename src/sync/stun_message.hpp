#pragma once

#include <QByteArray>

namespace peerchalk::sync {

// The smallest STUN exchange (RFC 5389): a Binding request, which STUN and TURN servers
// both answer. Used to check that such a server is reachable over UDP.

// A Binding request carrying the 12-byte transaction id; throws std::invalid_argument for
// another size.
[[nodiscard]] QByteArray stunBindingRequest(const QByteArray &transactionId);
// Whether a datagram is a Binding success or error response to that transaction: either
// shows that the server answers.
[[nodiscard]] bool isStunBindingAnswer(const QByteArray &datagram, const QByteArray &transactionId);

} // namespace peerchalk::sync
