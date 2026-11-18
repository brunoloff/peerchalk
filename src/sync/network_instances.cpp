// Network: keeping this device's other app instances in step, directly and at once.
#include "sync/instance_link.hpp"
#include "sync/network.hpp"

#include "ui/library.hpp"

#include <QJsonObject>
#include <QtLogging>

#include <stdexcept>
#include <utility>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr auto instanceFlushDelay = std::chrono::milliseconds(16);
const Bytes emptyBoardState{std::byte{0}};

QByteArray toByteArray(const Bytes &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

} // namespace

void Network::setUpInstanceLink() {
    if (instanceLink_ == nullptr)
        return;
    instanceFlushTimer_.setSingleShot(true);
    instanceFlushTimer_.setInterval(instanceFlushDelay);
    connect(&instanceFlushTimer_, &QTimer::timeout, this, &Network::flushToInstances);
    connect(instanceLink_, &InstanceLink::received, this, &Network::receiveFromInstance);
    connect(instanceLink_, &InstanceLink::instanceJoined, this, &Network::instanceJoined);
    connect(&library_, &Library::boardChanged, this, [this](const QUuid &board) {
        instanceDirtyBoards_.insert(board);
        instanceFlushTimer_.start();
    });
    const auto personalChanged = [this] {
        instancePersonalDirty_ = true;
        instanceFlushTimer_.start();
    };
    connect(&library_, &Library::workspaceChanged, this, personalChanged);
    connect(&library_, &Library::contactsChanged, this, personalChanged);
    connect(&library_, &Library::profileChanged, this, personalChanged);
    sendToInstances(InstanceHello{});
}

// Every message names the user: instances may be signed in to different accounts.
void Network::sendToInstances(const Message &message) {
    instanceLink_->send(library_.workspace().user() + encode(message));
}

// Someone new may have missed everything: send what is in memory again. Merging is
// idempotent, and what was already saved they read when they started.
void Network::instanceJoined() {
    instanceSentState_.clear();
    for (const QUuid &board : library_.loadedBoards())
        instanceDirtyBoards_.insert(board);
    instancePersonalDirty_ = true;
    instanceFlushTimer_.start();
}

void Network::flushToInstances() {
    if (instancePersonalDirty_) {
        instancePersonalDirty_ = false;
        sendToInstances(Personal{library_.workspace().exportPersonal(),
                                 library_.identityStore().exportContacts()});
        sendToInstances(Profile{library_.displayName(), library_.colorIndex()});
    }
    for (const QUuid &id : std::exchange(instanceDirtyBoards_, {})) {
        if (!library_.workspace().hasBoard(id))
            continue;
        const Board &board = library_.document(id).board();
        const auto sent = instanceSentState_.find(id);
        const Bytes update =
            board.difference(sent != instanceSentState_.end() ? sent->second : emptyBoardState);
        instanceSentState_[id] = board.stateVector();
        sendToInstances(BoardUpdate{id, toByteArray(update)});
    }
}

void Network::applyInstanceBoard(const BoardUpdate &update) {
    if (!library_.workspace().hasBoard(update.board))
        return;
    Board &board = library_.document(update.board).board();
    const Bytes before = board.stateVector();
    const auto *data = reinterpret_cast<const std::byte *>(update.update.constData());
    if (!board.apply(ByteView(data, static_cast<std::size_t>(update.update.size()))))
        return;
    // What just arrived need not go back out, if nothing else was waiting to be sent.
    const auto sent = instanceSentState_.find(update.board);
    if (sent != instanceSentState_.end() && sent->second == before)
        sent->second = board.stateVector();
}

void Network::applyInstancePersonal(const Personal &personal) {
    try {
        if (library_.identityStore().mergeContacts(personal.contacts))
            library_.contactsUpdated();
        if (library_.workspace().mergePersonal(personal.workspace))
            library_.workspaceUpdated();
    } catch (const std::exception &error) {
        qWarning("PeerChalk: ignoring data from another instance: %s", error.what());
    }
}

void Network::receiveFromInstance(const QByteArray &bytes) {
    const QByteArray user = library_.workspace().user();
    if (!bytes.startsWith(user))
        return; // an instance signed in to another account
    const std::optional<Message> message = decode(bytes.mid(user.size()));
    if (!message)
        return;
    if (std::holds_alternative<InstanceHello>(*message))
        instanceJoined();
    else if (const auto *update = std::get_if<BoardUpdate>(&*message))
        applyInstanceBoard(*update);
    else if (const auto *personal = std::get_if<Personal>(&*message))
        applyInstancePersonal(*personal);
    else if (const auto *profile = std::get_if<Profile>(&*message))
        library_.applyInstanceProfile(profile->displayName, profile->colorIndex);
}

} // namespace peerchalk::sync
