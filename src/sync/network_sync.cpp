// Network: what authenticated peers exchange, and who may receive which board.
#include "sync/network.hpp"

#include "library/codes.hpp"
#include "library/secrets.hpp"
#include "ui/library.hpp"

#include <QtLogging>

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

QByteArray keyBytes(const SigningPublicKey &key) { return toBytes(key); }

AccessChanges accessChanges(const std::vector<SignedOp> &ops) {
    AccessChanges message;
    for (const SignedOp &op : ops)
        message.ops.emplace_back(op.payload, op.signature);
    return message;
}

} // namespace

void Network::receive(const PeerId &id, std::uint64_t generation, const QByteArray &bytes) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr)
        return;
    const std::optional<Message> message = decode(bytes);
    if (message && std::holds_alternative<MessagePart>(*message))
        receivePart(id, *peer, std::get<MessagePart>(*message));
    else
        dispatch(id, *peer, message);
}

// Only someone who has signed in may send parts, and only people with access much data.
void Network::receivePart(const PeerId &id, Peer &peer, const MessagePart &part) {
    const qsizetype limit = peerHasAccess(peer) ? maxMessageBytes : maxUntrustedMessageBytes;
    const auto status =
        peer.user ? peer.assembler.add(part, limit) : MessageAssembler::Status::Invalid;
    if (status == MessageAssembler::Status::Invalid) {
        disconnectPeer(id, peer.generation);
    } else if (status == MessageAssembler::Status::Complete) {
        const std::optional<Message> message = decode(peer.assembler.take());
        if (message && std::holds_alternative<MessagePart>(*message))
            disconnectPeer(id, peer.generation);
        else
            dispatch(id, peer, message);
    }
}

void Network::dispatch(const PeerId &id, Peer &peer, const std::optional<Message> &message) {
    const bool hello = message && std::holds_alternative<Hello>(*message);
    // A device must say who it is first, and only once; anything malformed ends the call.
    if (!message || hello == peer.user.has_value()) {
        disconnectPeer(id, peer.generation);
        return;
    }
    if (hello) {
        if (!handleHello(peer, std::get<Hello>(*message))) {
            disconnectPeer(id, peer.generation);
            return;
        }
        peerAuthenticated(id, peer);
    } else if (const auto *changes = std::get_if<AccessChanges>(&*message)) {
        handleAccess(*changes);
    } else if (const auto *personal = std::get_if<Personal>(&*message)) {
        handlePersonal(peer, *personal);
    } else if (const auto *update = std::get_if<BoardUpdate>(&*message)) {
        handleBoardUpdate(peer, *update);
    } else if (const auto *join = std::get_if<BoardLinkJoin>(&*message)) {
        handleLinkJoin(peer, *join);
    } else if (const auto *state = std::get_if<BoardState>(&*message)) {
        handleBoardState(id, peer, *state);
    } else if (const auto *presence = std::get_if<Presence>(&*message)) {
        applyPresence(peer, *presence);
    } else if (const auto *profile = std::get_if<Profile>(&*message)) {
        peer.user->name = profile->displayName.trimmed();
        peer.user->colorIndex = profile->colorIndex;
        emit presenceChanged();
        emit peersChanged();
    } // anything else comes from a newer version and is ignored
}

bool Network::peerHasAccess(const Peer &peer) const {
    if (!peer.user)
        return false;
    const Space &space = *spaces_.at(peer.space);
    const Workspace &workspace = library_.workspace();
    switch (space.kind) {
    case SpaceKind::Personal:
        return true; // handleHello accepted only this same user
    case SpaceKind::Project:
        return workspace.mayAccessProject(peer.user->userKey, space.id);
    case SpaceKind::Board:
        return workspace.mayAccessBoard(peer.user->userKey, space.id);
    }
    return false;
}

// Accepts the device if its certificate is valid for the key p2pdc authenticated, and,
// in the personal space, only if it acts for this same user.
bool Network::handleHello(Peer &peer, const Hello &hello) {
    const std::optional<DeviceCertificate> certificate = verifyDeviceCertificate(
        hello.certificatePayload, hello.certificateSignature, keyBytes(peer.device));
    if (!certificate)
        return false;
    if (!protocolsCompatible(hello)) {
        markIncompatible(peer.device, hello.minProtocol > protocolVersion);
        return false;
    }
    const bool sameUser = certificate->userKey == library_.workspace().user();
    if (spaces_.at(peer.space)->kind == SpaceKind::Personal && !sameUser)
        return false;
    peer.user = RemoteUser{certificate->userKey, hello.displayName.trimmed(), hello.colorIndex};
    return true;
}

void Network::peerAuthenticated(const PeerId &id, Peer &peer) {
    const Space &space = *spaces_.at(peer.space);
    const bool mayUseLink =
        space.kind == SpaceKind::Board && library_.workspace().boardHasLink(space.id);
    if (const std::optional<QString> item = requestedItemName(peer); item && !mayUseLink)
        emit joinRequested(peer.user->name, *item);
    offerAccess(peer);
    offerLinkJoin(peer);
    offerBoardStates(peer);
    for (const QUuid &board : syncableBoards(peer))
        peer.dirtyBoards.insert(board);
    peer.presenceDirty = true;
    flush(id);
    emit peersChanged();
}

// A space's signed access changes go to anyone holding its capability (they reveal names
// and roles, not content), so they spread even when the people who signed them are
// offline; a user's own devices get everything, including capabilities.
void Network::offerAccess(Peer &peer) {
    const Space &space = *spaces_.at(peer.space);
    Workspace &workspace = library_.workspace();
    if (space.kind == SpaceKind::Personal) {
        queue(peer, accessChanges(workspace.allOps()));
        queue(peer,
              Personal{workspace.exportPersonal(), library_.identityStore().exportContacts()});
    } else if (space.kind == SpaceKind::Project) {
        queue(peer, accessChanges(workspace.opsForProject(space.id)));
    } else {
        queue(peer, accessChanges(workspace.opsForBoard(space.id)));
    }
}

void Network::handleAccess(const AccessChanges &message) {
    if (library_.workspace().acceptOps(message.ops))
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
             .apply(ByteView(data, static_cast<std::size_t>(update.update.size())))) {
        qWarning("PeerChalk: ignoring a malformed board update");
        return;
    }
    settleLiveStrokes(peer, update.board);
}

bool Network::sharesBoard(const Peer &peer, const QUuid &board) const {
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

bool Network::syncable(const Peer &peer, const QUuid &board) const {
    if (!sharesBoard(peer, board))
        return false;
    const Workspace &workspace = library_.workspace();
    return workspace.isBoardOwner(workspace.user(), board) ||
           workspace.isBoardOwner(peer.user->userKey, board);
}

bool Network::sharesLiveActivity(const Peer &peer, const QUuid &board) const {
    return sharesBoard(peer, board) && boardLive(board);
}

std::vector<QUuid> Network::syncableBoards(const Peer &peer) const {
    std::vector<QUuid> result;
    for (const BoardEntry &board : library_.workspace().boards())
        if (syncable(peer, board.id))
            result.push_back(board.id);
    return result;
}

// Access or projects changed: open or close spaces, re-offer access changes and send any
// boards that became shared.
void Network::workspaceChanged() {
    reconcileSpaces();
    for (auto &[id, peer] : peers_) {
        if (!peer->ready || !peer->user)
            continue;
        offerAccess(*peer);
        offerLinkJoin(*peer);
        offerBoardStates(*peer);
        for (const QUuid &board : syncableBoards(*peer))
            if (!peer->sentBoards.contains(board))
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

// Your name or colour changed: tell everyone already connected.
void Network::profileChanged() {
    for (auto &[id, peer] : peers_) {
        if (peer->ready && peer->user) {
            queue(*peer, Profile{library_.displayName(), library_.colorIndex()});
            flush(id);
        }
    }
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
                                  {"cursor", peer->presence->cursorVisible},
                                  {"view", peer->presence->view}});
    }
    return result;
}

std::optional<QString> Network::requestedItemName(const Peer &peer) const {
    const Space &space = *spaces_.at(peer.space);
    const Workspace &workspace = library_.workspace();
    if (!peer.user || space.kind == SpaceKind::Personal)
        return std::nullopt;
    const QByteArray me = workspace.user();
    const QByteArray them = peer.user->userKey;
    if (space.kind == SpaceKind::Project) {
        if (workspace.projectRole(space.id, me) < Role::Owner ||
            workspace.mayAccessProject(them, space.id))
            return std::nullopt;
        return workspace.project(space.id).name();
    }
    if (workspace.boardRole(space.id, me) < Role::Owner || workspace.mayAccessBoard(them, space.id))
        return std::nullopt;
    return workspace.board(space.id).name();
}

QVariantList Network::joinRequests(const QString &itemId) const {
    const QUuid item = QUuid::fromString(itemId);
    QVariantList result;
    std::set<QByteArray> seen; // a user may connect from several devices
    for (const auto &[id, peer] : peers_) {
        if (!peer->ready || spaces_.at(peer->space)->id != item || !requestedItemName(*peer) ||
            !seen.insert(peer->user->userKey).second)
            continue;
        result.append(QVariantMap{{"key", QString::fromLatin1(peer->user->userKey.toHex())},
                                  {"name", peer->user->name},
                                  {"fingerprint", formatFingerprint(peer->user->userKey)}});
    }
    return result;
}

int Network::joinRequestCount() const {
    std::set<std::pair<QUuid, QByteArray>> requests;
    for (const auto &[id, peer] : peers_)
        if (peer->ready && requestedItemName(*peer))
            requests.insert({spaces_.at(peer->space)->id, peer->user->userKey});
    return static_cast<int>(requests.size());
}

QVariantList Network::allJoinRequests() const {
    QVariantList result;
    std::set<std::pair<QUuid, QByteArray>> seen;
    for (const auto &[id, peer] : peers_) {
        if (!peer->ready)
            continue;
        const std::optional<QString> item = requestedItemName(*peer);
        const Space &space = *spaces_.at(peer->space);
        if (!item || !seen.insert({space.id, peer->user->userKey}).second)
            continue;
        result.append(QVariantMap{{"itemId", space.id.toString(QUuid::WithoutBraces)},
                                  {"itemName", *item},
                                  {"board", space.kind == SpaceKind::Board},
                                  {"key", QString::fromLatin1(peer->user->userKey.toHex())},
                                  {"name", peer->user->name},
                                  {"fingerprint", formatFingerprint(peer->user->userKey)}});
    }
    return result;
}

} // namespace peerchalk::sync
