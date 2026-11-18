// Network: what may be lost on the way. Pointer positions and strokes being drawn go out
// as p2pdc datagrams, so they never wait behind board updates. Anything that must arrive
// (a pointer that stopped moving or left, a board change) still goes over the reliable
// channel, which also carries presence to peers without datagram support.
#include "library/secrets.hpp"
#include "sync/network.hpp"
#include "ui/library.hpp"

#include <QRandomGenerator>

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr auto liveStrokeInterval = std::chrono::milliseconds(33);
constexpr auto liveExpiryInterval = std::chrono::milliseconds(500);
constexpr auto presenceSettleDelay = std::chrono::milliseconds(400);
constexpr int finalDatagramRepeats = 3; // the end of a stroke matters more

ByteView byteView(const QByteArray &bytes) {
    return {reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

} // namespace

void Network::setUpLiveUpdates() {
    nextLiveStroke_ = QRandomGenerator::global()->generate();
    liveStrokeTimer_.setSingleShot(true);
    liveStrokeTimer_.setInterval(liveStrokeInterval);
    connect(&liveStrokeTimer_, &QTimer::timeout, this, &Network::sendLiveStroke);
    liveExpiryTimer_.setInterval(liveExpiryInterval);
    connect(&liveExpiryTimer_, &QTimer::timeout, this, &Network::expireLiveStrokes);
    presenceSettleTimer_.setSingleShot(true);
    presenceSettleTimer_.setInterval(presenceSettleDelay);
    connect(&presenceSettleTimer_, &QTimer::timeout, this, [this] { sendPresence(true); });
    library_.setDraftListener([this](const QUuid &board, const Stroke &stroke, DraftState state) {
        draftUpdated(board, stroke, state);
    });
}

void Network::setPresence(const QString &boardId, QPointF cursor, bool cursorVisible) {
    const QUuid board = QUuid::fromString(boardId);
    presenceMustArrive_ =
        presenceMustArrive_ || board != presenceBoard_ || cursorVisible != cursorVisible_;
    if (board != presenceBoard_)
        presenceView_ = QRectF();
    presenceBoard_ = board;
    cursor_ = cursor;
    cursorVisible_ = cursorVisible;
    ++presenceSequence_;
    presenceSettleTimer_.start();
    if (!presenceTimer_.isActive())
        presenceTimer_.start();
}

void Network::setPresenceView(const QString &boardId, const QRectF &view) {
    if (QUuid::fromString(boardId) != presenceBoard_ || view == presenceView_)
        return;
    presenceView_ = view;
    ++presenceSequence_;
    presenceSettleTimer_.start();
    if (!presenceTimer_.isActive())
        presenceTimer_.start();
}

void Network::sendPresence(bool mustArrive) {
    const bool reliable = mustArrive || presenceMustArrive_;
    presenceMustArrive_ = false;
    for (auto &[id, peer] : peers_) {
        if (peer->ready && peer->user) {
            peer->presenceDirty = true;
            peer->presenceReliable = peer->presenceReliable || reliable;
            flush(id);
        }
    }
}

// Sends this peer's pending presence, or leaves it pending until the peer can take more.
void Network::flushPresence(const PeerId &id, Peer &peer) {
    const QByteArray bytes = encode(presenceFor(peer));
    if (!peer.presenceReliable && peer.connection->datagrams_available() &&
        peer.connection->send_datagram(byteView(bytes)) == SendStatus::Accepted) {
        peer.presenceDirty = false;
        return;
    }
    if (sendOne(id, peer, bytes)) {
        peer.presenceDirty = false;
        peer.presenceReliable = false;
    }
}

// Peers who may not see the open board only learn that no board is open.
Presence Network::presenceFor(const Peer &peer) const {
    if (presenceBoard_.isNull() || !sharesLiveActivity(peer, presenceBoard_))
        return {QUuid(), QPointF(), false, presenceSequence_};
    return {presenceBoard_, cursor_, cursorVisible_, presenceSequence_, presenceView_};
}

void Network::applyPresence(Peer &peer, const Presence &presence) {
    // A datagram can arrive after newer news; senders without sequence numbers send 0.
    if (peer.presence && presence.sequence != 0 && presence.sequence < peer.presence->sequence)
        return;
    peer.presence = presence;
    emit presenceChanged();
}

void Network::receiveDatagram(const PeerId &id, std::uint64_t generation, const QByteArray &bytes) {
    Peer *peer = livePeer(id, generation);
    if (peer == nullptr || !peer->ready || !peer->user)
        return;
    const std::optional<Message> message = decode(bytes);
    if (!message)
        return;
    if (const auto *presence = std::get_if<Presence>(&*message))
        applyPresence(*peer, *presence);
    else if (const auto *stroke = std::get_if<LiveStroke>(&*message))
        handleLiveStroke(*peer, *stroke);
}

void Network::draftUpdated(const QUuid &board, const Stroke &stroke, DraftState state) {
    if (!drafting_)
        ++nextLiveStroke_;
    drafting_ = state == DraftState::Drawing;
    const std::vector<StrokePoint> &points = stroke.points();
    const std::size_t first =
        points.size() > maxLiveStrokePoints ? points.size() - maxLiveStrokePoints : 0;
    outgoingStroke_ =
        LiveStroke{board,
                   nextLiveStroke_,
                   static_cast<quint32>(first),
                   state,
                   stroke.argb(),
                   stroke.width(),
                   stroke.kind(),
                   {points.begin() + static_cast<std::ptrdiff_t>(first), points.end()}};
    outgoingRepeats_ = drafting_ ? 1 : finalDatagramRepeats;
    // The end goes out at once, ahead of the board update that follows it, so that the
    // update can replace the preview.
    if (!drafting_)
        sendLiveStroke();
    else if (!liveStrokeTimer_.isActive())
        liveStrokeTimer_.start();
}

void Network::sendLiveStroke() {
    if (!outgoingStroke_)
        return;
    const QByteArray bytes = encode(*outgoingStroke_);
    std::set<SigningPublicKey> sent; // a device may be connected through several spaces
    for (const auto &[id, peer] : peers_) {
        if (peer->ready && sharesLiveActivity(*peer, outgoingStroke_->board) &&
            peer->connection->datagrams_available() && !sent.contains(peer->device) &&
            peer->connection->send_datagram(byteView(bytes)) == SendStatus::Accepted)
            sent.insert(peer->device);
    }
    if (--outgoingRepeats_ > 0)
        liveStrokeTimer_.start();
    else
        outgoingStroke_.reset();
}

void Network::handleLiveStroke(const Peer &peer, const LiveStroke &stroke) {
    if (stroke.board != presenceBoard_ || !sharesLiveActivity(peer, stroke.board))
        return;
    const auto change =
        liveStrokes_.apply(toBytes(peer.device), stroke, LiveStrokeTracker::Clock::now());
    if (change)
        applyLiveChange(*change);
    if (!liveExpiryTimer_.isActive())
        liveExpiryTimer_.start();
}

void Network::applyLiveChange(const LiveStrokeTracker::Change &change) {
    const Workspace &workspace = library_.workspace();
    if (!workspace.hasBoard(change.board) || workspace.board(change.board).deleted())
        return;
    BoardDocument &document = library_.document(change.board);
    if (change.stroke)
        document.setRemoteDraft(change.key, *change.stroke);
    else
        document.removeRemoteDraft(change.key);
}

void Network::settleLiveStrokes(const Peer &peer, const QUuid &board) {
    for (const auto &change : liveStrokes_.settle(toBytes(peer.device), board))
        applyLiveChange(change);
}

void Network::expireLiveStrokes() {
    for (const auto &change : liveStrokes_.expire(LiveStrokeTracker::Clock::now()))
        applyLiveChange(change);
    if (liveStrokes_.empty())
        liveExpiryTimer_.stop();
}

} // namespace peerchalk::sync
