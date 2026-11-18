// Network: keeping boards in step with each peer, sending only what it lacks.
#include "sync/network.hpp"

#include "ui/library.hpp"

#include <QtLogging>

#include <algorithm>
#include <stdexcept>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr std::size_t maxKnownBoardStates = 4096;

// A Yrs v1 update that changes nothing: no structs and an empty delete set.
bool isEmptyUpdate(const Bytes &update) {
    return update.size() <= 2 &&
           std::ranges::all_of(update, [](std::byte b) { return b == std::byte{0}; });
}

QByteArray toByteArray(const Bytes &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

} // namespace

// Tells the peer what this device has of each shared board, so it sends only the rest.
void Network::offerBoardStates(Peer &peer) {
    for (const QUuid &board : syncableBoards(peer))
        if (peer.offeredStates.insert(board).second)
            queue(peer,
                  BoardState{board, toByteArray(library_.document(board).board().stateVector())});
}

// The state is kept even for a board not shared yet: access may arrive just after it.
// Whether anything is sent is decided when sending.
void Network::handleBoardState(const PeerId &id, Peer &peer, const BoardState &state) {
    if (peer.sentState.size() >= maxKnownBoardStates && !peer.sentState.contains(state.board))
        return;
    const auto *data = reinterpret_cast<const std::byte *>(state.stateVector.constData());
    peer.sentState[state.board] = Bytes(data, data + state.stateVector.size());
    if (syncable(peer, state.board)) {
        peer.dirtyBoards.insert(state.board);
        flush(id);
    }
}

// Someone with a board's link asks its owners to let them in, once they know who those are.
void Network::offerLinkJoin(Peer &peer) {
    const Space &space = *spaces_.at(peer.space);
    const Workspace &workspace = library_.workspace();
    if (peer.linkJoinSent || !peer.user || space.kind != SpaceKind::Board ||
        !workspace.hasBoard(space.id))
        return;
    const BoardEntry &board = workspace.board(space.id);
    // The token goes to verified owners only: anyone else here could reuse it.
    if (board.linkToken.isEmpty() || workspace.mayAccessBoard(workspace.user(), space.id) ||
        board.state.role(peer.user->userKey) < Role::Owner)
        return;
    peer.linkJoinSent = true;
    queue(peer, BoardLinkJoin{space.id, board.linkToken});
}

void Network::handleLinkJoin(const Peer &peer, const BoardLinkJoin &join) {
    const Space &space = *spaces_.at(peer.space);
    if (peer.user && space.kind == SpaceKind::Board && space.id == join.board)
        library_.admitByLink(join.board, peer.user->userKey, peer.user->name, join.token);
}

bool Network::boardLive(const QUuid &board) const {
    const Workspace &workspace = library_.workspace();
    if (workspace.isBoardOwner(workspace.user(), board))
        return true;
    return std::ranges::any_of(peers_, [&](const auto &entry) {
        const Peer &peer = *entry.second;
        return peer.ready && peer.user && sharesBoard(peer, board) &&
               workspace.isBoardOwner(peer.user->userKey, board);
    });
}

QString Network::boardAccess(const QString &boardId) const {
    const QUuid board = QUuid::fromString(boardId);
    const Workspace &workspace = library_.workspace();
    if (!workspace.mayAccessBoard(workspace.user(), board))
        return QStringLiteral("waiting");
    return boardLive(board) ? QStringLiteral("edit") : QStringLiteral("offline");
}

// Queues the changes of one board the peer lacks; false when there is nothing to send.
bool Network::queueNextBoard(const PeerId &id, Peer &peer) {
    for (auto it = peer.dirtyBoards.begin(); it != peer.dirtyBoards.end();) {
        const QUuid board = *it;
        if (!syncable(peer, board)) {
            it = peer.dirtyBoards.erase(it);
            continue;
        }
        const auto known = peer.sentState.find(board);
        if (known == peer.sentState.end()) {
            ++it; // waits until the peer says what it has
            continue;
        }
        it = peer.dirtyBoards.erase(it);
        const Board &document = library_.document(board).board();
        Bytes update;
        try {
            update = document.difference(known->second);
        } catch (const std::invalid_argument &) {
            qWarning("PeerChalk: a peer sent an unusable board state");
            disconnectPeer(id, peer.generation);
            return false;
        }
        peer.sentBoards.insert(board);
        const QByteArray encoded = encode(BoardUpdate{board, toByteArray(update)});
        if (encoded.size() > maxMessageBytes) {
            qWarning("PeerChalk: a board update of %lld bytes is too large to send",
                     static_cast<long long>(encoded.size()));
            continue; // the state stays as it was, so a later change tries again
        }
        known->second = document.stateVector();
        if (isEmptyUpdate(update))
            continue;
        for (QByteArray &part : splitMessage(encoded))
            peer.outbox.push_back(std::move(part));
        return true;
    }
    return false;
}

} // namespace peerchalk::sync
