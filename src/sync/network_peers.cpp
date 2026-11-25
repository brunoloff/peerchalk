// Network: connections, the outbox and sending board changes.
#include "sync/connection_log.hpp"
#include "sync/network.hpp"
#include "sync/p2pdc_names.hpp"
#include "sync/repository_scribe.hpp"

#include "ui/library.hpp"

#include <QtLogging>

#include <utility>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

const QString connectionArea = QStringLiteral("connection");

} // namespace

// Larger messages travel in parts (see message_parts.hpp). p2pdc caps pending bytes at
// 4 MiB, which must hold at least one message.
ConnectOptions Network::connectOptions() {
    ConnectOptions options;
    options.max_message_bytes = 2 * 1024 * 1024;
    options.max_pending_bytes = 4 * 1024 * 1024;
    return options;
}

void Network::connectPeer(const QString &space, const Advertisement &advertisement) {
    const PeerId id{space, advertisement.signing_public_key};
    const auto generation = ++nextGeneration_;
    Space &found = *spaces_.at(space);
    auto result = node_->connect(*found.lease, *found.watch, advertisement, config_.ice,
                                 scheduler(), connectOptions(), handlers(id, generation));
    auto *connection = std::get_if<std::unique_ptr<Connection>>(&result);
    const QString who =
        QStringLiteral("%1: device %2").arg(spaceLabel(space), shortDevice(id.second));
    if (connection == nullptr) {
        logWarning(connectionArea, QStringLiteral("%1: could not dial: %2")
                                       .arg(who, describe(std::get<ConnectError>(result))));
        return; // retried by the retry timer
    }
    logInfo(connectionArea, QStringLiteral("%1: dialing").arg(who));
    addPeer(id, generation, std::move(*connection));
}

void Network::addPeer(const PeerId &id, std::uint64_t generation,
                      std::unique_ptr<Connection> connection) {
    auto peer = std::make_unique<Peer>();
    peer->space = id.first;
    peer->device = id.second;
    peer->generation = generation;
    peer->connection = std::move(connection);
    peer->started.start();
    peers_[id] = std::move(peer);
}

// p2pdc reports a route when it or its round-trip time changes; only the former is logged.
void Network::routeChanged(const PeerId &id, std::uint64_t generation, const RouteInfo &route) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    const QString described = describe(route);
    if (std::exchange(peer->route, described) != described)
        logInfo(connectionArea, QStringLiteral("%1: route %2").arg(peerLabel(id), described));
    const bool kind = std::exchange(peer->routeKind, route.current) != route.current;
    const bool time = std::exchange(peer->roundTrip, route.round_trip_time) != route.round_trip_time;
    if (kind)
        emit peersChanged();
    else if (time)
        emit devicesChanged();
}

QString Network::peerLabel(const PeerId &id) const {
    return QStringLiteral("%1: device %2").arg(spaceLabel(id.first), shortDevice(id.second));
}

ConnectionHandlers Network::handlers(PeerId id, std::uint64_t generation) {
    return {
        [this, id, generation](ConnectionState state) { connectionState(id, generation, state); },
        [this, id, generation](RouteInfo route) { routeChanged(id, generation, route); },
        [this, id, generation](Bytes message) {
            receive(id, generation,
                    QByteArray(reinterpret_cast<const char *>(message.data()),
                               static_cast<qsizetype>(message.size())));
        },
        [this, id, generation] {
            if (livePeer(id, generation) != nullptr)
                flush(id);
        },
        [this, id, generation](ConnectError error) {
            if (livePeer(id, generation) != nullptr)
                logWarning(connectionArea,
                           QStringLiteral("%1: %2").arg(peerLabel(id), describe(error)));
            disconnectPeer(id, generation);
        },
        [this, id](ConnectionStatus) {
            logWarning(connectionArea,
                       QStringLiteral("%1: signaling server unreachable; waiting for it to return")
                           .arg(peerLabel(id)));
        },
        [this, id, generation](Bytes message) {
            receiveDatagram(id, generation,
                            QByteArray(reinterpret_cast<const char *>(message.data()),
                                       static_cast<qsizetype>(message.size())));
        }};
}

Network::Peer *Network::livePeer(const PeerId &id, std::uint64_t generation) {
    const auto it = peers_.find(id);
    if (it == peers_.end() || it->second->generation != generation)
        return nullptr;
    return it->second.get();
}

void Network::connectionState(const PeerId &id, std::uint64_t generation, ConnectionState state) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    logInfo(connectionArea, QStringLiteral("%1: %2 after %3 ms")
                                .arg(peerLabel(id), describe(state))
                                .arg(peer->started.elapsed()));
    if (state == ConnectionState::Ready) {
        peer->ready = true;
        const DeviceCertificate certificate =
            library_.identityStore().identity().deviceCertificate();
        queue(*peer, Hello{certificate.payload, certificate.signature, library_.displayName(),
                           library_.colorIndex()});
        flush(id);
    } else if (state == ConnectionState::Suspended) {
        peer->ready = false;
        dropDownloadsFrom(id); // it may not come back soon
    } else if (state == ConnectionState::Failed || state == ConnectionState::Closed) {
        disconnectPeer(id, generation);
    }
}

// Forgets the peer; the connection is destroyed outside its own callback.
void Network::disconnectPeer(const PeerId &id, std::uint64_t generation) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    logInfo(connectionArea, QStringLiteral("%1: disconnected after %2 s")
                                .arg(peerLabel(id))
                                .arg(peer->started.elapsed() / 1000));
    peer->ready = false;
    dropDownloadsFrom(id);
    QTimer::singleShot(0, this, [this, id, generation] {
        if (livePeer(id, generation) != nullptr)
            peers_.erase(id);
        repository_->reconsider();
        emit peersChanged();
        emit presenceChanged();
    });
}

int Network::connectedDevices() const {
    std::set<SigningPublicKey> devices;
    for (const auto &[id, peer] : peers_)
        if (peer->ready && peer->user)
            devices.insert(peer->device);
    return static_cast<int>(devices.size());
}

void Network::queue(Peer &peer, const Message &message) {
    for (QByteArray &part : splitMessage(encode(message)))
        peer.outbox.push_back(std::move(part));
}

// Sends what is pending in order: queued messages, board changes, then presence.
void Network::flush(const PeerId &id) {
    const auto it = peers_.find(id);
    if (it == peers_.end() || !it->second->ready || !it->second->connection)
        return;
    Peer &peer = *it->second;
    do {
        while (!peer.outbox.empty()) {
            if (!sendOne(id, peer, peer.outbox.front()))
                return;
            peer.outbox.pop_front();
        }
    } while (peer.user && queueNextBoard(id, peer));
    if (peer.ready && peer.user && peer.presenceDirty)
        flushPresence(id, peer);
}

// Returns true when the message was accepted or must be dropped; false to retry later.
bool Network::sendOne(const PeerId &id, Peer &peer, const QByteArray &bytes) {
    const auto status =
        peer.connection->send(ByteView(reinterpret_cast<const std::byte *>(bytes.constData()),
                                       static_cast<std::size_t>(bytes.size())));
    switch (status) {
    case SendStatus::Accepted:
        return true;
    case SendStatus::Backpressure:
    case SendStatus::NotReady:
        return false; // flushed again from on_writable or the next change
    case SendStatus::TooLarge:
        qWarning("PeerChalk: a message of %lld bytes is too large to send",
                 static_cast<long long>(bytes.size()));
        return true;
    case SendStatus::Failed:
    case SendStatus::Closed:
        disconnectPeer(id, peer.generation);
        return false;
    }
    return false;
}

} // namespace peerchalk::sync
