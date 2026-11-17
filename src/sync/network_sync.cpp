// Network: what authenticated peers exchange, and who may receive which board.
#include "sync/network.hpp"

#include "library/secrets.hpp"
#include "ui/library.hpp"

#include <QtLogging>

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

QByteArray keyBytes(const SigningPublicKey &key) { return toBytes(key); }

} // namespace

void Network::receive(const PeerId &id, std::uint64_t generation, const QByteArray &bytes) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    const std::optional<Message> message = decode(bytes);
    const bool hello = message && std::holds_alternative<Hello>(*message);
    // A device must say who it is first, and only once; anything malformed ends the call.
    if (!message || hello == peer->user.has_value()) {
        disconnectPeer(id, generation);
        return;
    }
    if (hello) {
        if (!handleHello(*peer, std::get<Hello>(*message))) {
            disconnectPeer(id, generation);
            return;
        }
        peerAuthenticated(id, *peer);
    } else if (const auto *manifests = std::get_if<Manifests>(&*message)) {
        handleManifests(*manifests);
    } else if (const auto *personal = std::get_if<Personal>(&*message)) {
        handlePersonal(*peer, *personal);
    } else if (const auto *update = std::get_if<BoardUpdate>(&*message)) {
        handleBoardUpdate(*peer, *update);
    } else if (const auto *presence = std::get_if<Presence>(&*message)) {
        peer->presence = *presence;
        emit presenceChanged();
    }
}

// Accepts the device if its certificate is valid for the key p2pdc authenticated, and,
// in the personal space, only if it acts for this same user.
bool Network::handleHello(Peer &peer, const Hello &hello) {
    const std::optional<DeviceCertificate> certificate = verifyDeviceCertificate(
        hello.certificatePayload, hello.certificateSignature, keyBytes(peer.device));
    if (!certificate)
        return false;
    const bool sameUser = certificate->userKey == library_.workspace().user();
    if (spaces_.at(peer.space)->kind == SpaceKind::Personal && !sameUser)
        return false;
    peer.user = RemoteUser{certificate->userKey, hello.displayName.trimmed(), hello.colorIndex};
    return true;
}

void Network::peerAuthenticated(const PeerId &id, Peer &peer) {
    offerManifests(peer);
    for (const QUuid &board : syncableBoards(peer))
        peer.dirtyBoards.insert(board);
    peer.presenceDirty = true;
    flush(id);
    emit peersChanged();
}

// Manifests of the space go to anyone holding its capability (they reveal names and
// members, not content); a user's own devices get everything, including capabilities.
void Network::offerManifests(Peer &peer) {
    const Space &space = *spaces_.at(peer.space);
    Workspace &workspace = library_.workspace();
    if (space.kind == SpaceKind::Personal) {
        queue(peer, Manifests{workspace.allManifests()});
        queue(peer,
              Personal{workspace.exportPersonal(), library_.identityStore().exportContacts()});
    } else if (space.kind == SpaceKind::Project) {
        queue(peer, Manifests{workspace.manifestsForProject(space.id)});
    } else {
        queue(peer, Manifests{workspace.manifestsForBoard(space.id)});
    }
}

void Network::handleManifests(const Manifests &message) {
    bool changed = false;
    for (const SignedManifest &manifest : message.manifests)
        changed |= library_.workspace().acceptManifest(manifest.payload, manifest.signature);
    if (changed)
        library_.workspaceUpdated(); // also re-evaluates spaces and boards via workspaceChanged
}

void Network::handlePersonal(const Peer &peer, const Personal &message) {
    if (spaces_.at(peer.space)->kind != SpaceKind::Personal)
        return;
    try {
        if (library_.identityStore().mergeContacts(message.contacts))
            library_.contactsUpdated();
        if (library_.workspace().mergePersonal(message.workspace))
            library_.workspaceUpdated();
    } catch (const std::exception &error) {
        qWarning("PeerChalk: ignoring personal data from another device: %s", error.what());
    }
}

void Network::handleBoardUpdate(const Peer &peer, const BoardUpdate &update) {
    if (!syncable(peer, update.board))
        return; // not shared with this peer; ignore rather than trust it
    const auto *data = reinterpret_cast<const std::byte *>(update.update.constData());
    if (!library_.document(update.board)
             .board()
             .apply(ByteView(data, static_cast<std::size_t>(update.update.size()))))
        qWarning("PeerChalk: ignoring a malformed board update");
}

bool Network::syncable(const Peer &peer, const QUuid &board) const {
    if (!peer.user)
        return false;
    const Workspace &workspace = library_.workspace();
    const QByteArray me = workspace.user();
    if (!workspace.mayAccessBoard(me, board))
        return false;
    const Space &space = *spaces_.at(peer.space);
    switch (space.kind) {
    case SpaceKind::Personal:
        return true;
    case SpaceKind::Project: {
        const std::vector<QUuid> boards = workspace.boardsInProject(space.id);
        return std::ranges::find(boards, board) != boards.end() &&
               workspace.mayAccessBoard(peer.user->userKey, board);
    }
    case SpaceKind::Board:
        return board == space.id && workspace.mayAccessBoard(peer.user->userKey, board);
    }
    return false;
}

std::vector<QUuid> Network::syncableBoards(const Peer &peer) const {
    std::vector<QUuid> result;
    for (const BoardEntry &board : library_.workspace().boards())
        if (syncable(peer, board.id))
            result.push_back(board.id);
    return result;
}

// Membership or projects changed: open or close spaces, re-offer manifests and send any
// boards that became shared.
void Network::workspaceChanged() {
    reconcileSpaces();
    for (auto &[id, peer] : peers_) {
        if (!peer->ready || !peer->user)
            continue;
        offerManifests(*peer);
        for (const QUuid &board : syncableBoards(*peer))
            if (!peer->sentState.contains(board))
                peer->dirtyBoards.insert(board);
        flush(id);
    }
    emit presenceChanged();
}

void Network::boardChanged(const QUuid &board) {
    for (auto &[id, peer] : peers_) {
        if (peer->ready && syncable(*peer, board)) {
            peer->dirtyBoards.insert(board);
            flush(id);
        }
    }
}

void Network::setPresence(const QString &boardId, QPointF cursor, bool cursorVisible) {
    presenceBoard_ = QUuid::fromString(boardId);
    cursor_ = cursor;
    cursorVisible_ = cursorVisible;
    if (!presenceTimer_.isActive())
        presenceTimer_.start();
}

void Network::sendPresence() {
    for (auto &[id, peer] : peers_) {
        if (peer->ready && peer->user) {
            peer->presenceDirty = true;
            flush(id);
        }
    }
}

// Peers who may not see the open board only learn that no board is open.
Presence Network::presenceFor(const Peer &peer) const {
    if (presenceBoard_.isNull() || !syncable(peer, presenceBoard_))
        return {};
    return {presenceBoard_, cursor_, cursorVisible_};
}

QVariantList Network::boardPresence(const QString &boardId) const {
    const QUuid board = QUuid::fromString(boardId);
    QVariantList result;
    std::set<SigningPublicKey> seen; // a device may be connected through several spaces
    for (const auto &[id, peer] : peers_) {
        if (!peer->ready || !peer->user || !peer->presence || peer->presence->board != board ||
            !seen.insert(peer->device).second)
            continue;
        result.append(QVariantMap{{"device", QString::fromLatin1(keyBytes(peer->device).toHex())},
                                  {"name", peer->user->name},
                                  {"colorIndex", peer->user->colorIndex},
                                  {"x", peer->presence->cursor.x()},
                                  {"y", peer->presence->cursor.y()},
                                  {"cursor", peer->presence->cursorVisible}});
    }
    return result;
}

} // namespace peerchalk::sync
