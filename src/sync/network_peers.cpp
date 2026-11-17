// Network: connections, the outbox and sending board changes.
#include "sync/network.hpp"

#include "ui/library.hpp"

#include <QtLogging>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

// A Yrs v1 update that changes nothing: no structs and an empty delete set.
bool isEmptyUpdate(const Bytes &update) {
    return update.size() <= 2 &&
           std::ranges::all_of(update, [](std::byte b) { return b == std::byte{0}; });
}

const Bytes emptyStateVector{std::byte{0}};

} // namespace

// A board's first sync is one Yrs update of the whole board, so allow large messages
// (p2pdc caps pending bytes at 4 MiB, which must hold at least one message).
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
    if (connection == nullptr)
        return; // retried by the retry timer
    auto peer = std::make_unique<Peer>();
    peer->space = space;
    peer->device = advertisement.signing_public_key;
    peer->generation = generation;
    peer->connection = std::move(*connection);
    peers_[id] = std::move(peer);
}

ConnectionHandlers Network::handlers(PeerId id, std::uint64_t generation) {
    return {
        [this, id, generation](ConnectionState state) { connectionState(id, generation, state); },
        [](RouteInfo) {},
        [this, id, generation](Bytes message) {
            receive(id, generation,
                    QByteArray(reinterpret_cast<const char *>(message.data()),
                               static_cast<qsizetype>(message.size())));
        },
        [this, id, generation] {
            if (livePeer(id, generation) != nullptr)
                flush(id);
        },
        [this, id, generation](ConnectError) { disconnectPeer(id, generation); },
        [](ConnectionStatus) {}};
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
    if (state == ConnectionState::Ready) {
        peer->ready = true;
        const DeviceCertificate certificate =
            library_.identityStore().identity().deviceCertificate();
        queue(*peer, Hello{certificate.payload, certificate.signature, library_.displayName(),
                           library_.colorIndex()});
        flush(id);
    } else if (state == ConnectionState::Suspended) {
        peer->ready = false;
    } else if (state == ConnectionState::Failed || state == ConnectionState::Closed) {
        disconnectPeer(id, generation);
    }
}

// Forgets the peer; the connection is destroyed outside its own callback.
void Network::disconnectPeer(const PeerId &id, std::uint64_t generation) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    peer->ready = false;
    QTimer::singleShot(0, this, [this, id, generation] {
        if (livePeer(id, generation) != nullptr)
            peers_.erase(id);
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

void Network::queue(Peer &peer, const Message &message) { peer.outbox.push_back(encode(message)); }

// Sends what is pending in order: queued messages, board changes, then presence.
void Network::flush(const PeerId &id) {
    const auto it = peers_.find(id);
    if (it == peers_.end() || !it->second->ready || !it->second->connection)
        return;
    Peer &peer = *it->second;
    while (!peer.outbox.empty()) {
        if (!sendOne(id, peer, peer.outbox.front()))
            return;
        peer.outbox.pop_front();
    }
    if (!peer.user || !flushBoards(id, peer))
        return;
    if (peer.presenceDirty && sendOne(id, peer, encode(presenceFor(peer))))
        peer.presenceDirty = false;
}

// Sends each dirty board as the changes since what this peer was last sent.
bool Network::flushBoards(const PeerId &id, Peer &peer) {
    while (!peer.dirtyBoards.empty()) {
        const QUuid board = *peer.dirtyBoards.begin();
        if (!syncable(peer, board)) {
            peer.dirtyBoards.erase(board);
            continue;
        }
        const Board &document = library_.document(board).board();
        const auto sent = peer.sentState.find(board);
        const Bytes update =
            document.difference(sent != peer.sentState.end() ? sent->second : emptyStateVector);
        if (!isEmptyUpdate(update) &&
            !sendOne(
                id, peer,
                encode(BoardUpdate{board, QByteArray(reinterpret_cast<const char *>(update.data()),
                                                     static_cast<qsizetype>(update.size()))})))
            return false;
        peer.sentState[board] = document.stateVector();
        peer.dirtyBoards.erase(board);
    }
    return true;
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
