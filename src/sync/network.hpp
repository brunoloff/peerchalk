#pragma once

#include "board/stroke_codec.hpp"
#include "sync/live_strokes.hpp"
#include "sync/message_parts.hpp"
#include "sync/messages.hpp"
#include "sync/network_config.hpp"

#include <p2pdc/connection.h>
#include <p2pdc/discovery.h>
#include <p2pdc/incoming.h>
#include <p2pdc/node.h>

#include <QDateTime>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

class QQmlEngine;
class QJSEngine;

namespace peerchalk {
class Library;
}

namespace peerchalk::sync {

class InstanceLink;
class ServerChecks;
class ServerList;

// Connects this device with others over p2pdc and keeps shared data in step.
//
// Each project, each board with its own invite, and the user's own devices form a
// "space": devices holding its capability find each other through it and connect (the
// lower device key dials). The first message on a connection is a Hello carrying the
// device's certificate, which proves the user it acts for. Peers in a space then
// exchange that space's signed access changes, which anyone passes on; board contents
// flow only for boards that both users may access according to them, as Yrs updates relative to
// what was last sent. A user's own devices also exchange their workspace and contacts. Presence
// (which board is open, where the pointer is) goes to peers who may see that board.
//
// Everything runs on the Qt main thread, which is also the p2pdc executor.
class Network : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    // The node is running and looking for peers.
    Q_PROPERTY(bool online READ online NOTIFY statusChanged)
    Q_PROPERTY(int connectedDevices READ connectedDevices NOTIFY peersChanged)
    // People connected through an invite to a project or board you own who have no
    // access yet, across all of them.
    Q_PROPERTY(int joinRequestCount READ joinRequestCount NOTIFY requestsChanged)
    // PEERCHALK_* environment variables set, which take precedence over saved settings.
    Q_PROPERTY(bool settingsOverridden READ settingsOverridden CONSTANT)
    // Settings were saved that apply once PeerChalk starts again.
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY settingsChanged)
    // Someone here runs a version this one is too old to talk to.
    Q_PROPERTY(bool updateNeeded READ updateNeeded NOTIFY updateNeededChanged)
    // Changes when the connection log gets new entries (checked once a second).
    Q_PROPERTY(quint64 logRevision MEMBER logRevision_ NOTIFY logChanged)
    // Other devices, connected or connecting, one entry each: {device, name, colorIndex,
    // route}; route is "direct", "relay", "unknown" or "connecting".
    Q_PROPERTY(QVariantList devices READ devices NOTIFY peersChanged)
    // What the last server checks found, by "kind address": {state, milliseconds}; state is
    // "checking", "working" or "unreachable".
    Q_PROPERTY(QVariantMap serverStatus MEMBER serverStatus_ NOTIFY serverStatusChanged)

  public:
    // instanceLink, if any, keeps this device's other app instances in step.
    Network(Library &library, NetworkConfig config, QObject *parent = nullptr,
            InstanceLink *instanceLink = nullptr);
    ~Network() override;
    // Starts networking for the Library singleton, configured from the environment.
    static Network *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    void start();
    // Closes every connection and stops the node, waiting up to `timeout` for p2pdc to
    // finish; call before the application exits.
    void shutdown(std::chrono::milliseconds timeout);
    // The same without waiting here, for when the app keeps running: done is called on
    // the main thread once the network has stopped or timeout has passed.
    void shutdownThen(std::chrono::milliseconds timeout, std::function<void()> done);

    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool online() const { return started_ && !stopping_; }
    [[nodiscard]] int connectedDevices() const;
    [[nodiscard]] int joinRequestCount() const;
    [[nodiscard]] static bool settingsOverridden();
    [[nodiscard]] bool restartNeeded() const { return restartNeeded_; }
    [[nodiscard]] bool updateNeeded() const { return updateNeeded_; }
    // The saved settings: {enabled, bootstrap, dhtNetwork, stun, turn}.
    Q_INVOKABLE QVariantMap settings() const;
    // Checks and saves settings for the next start; returns what is wrong, or "".
    Q_INVOKABLE QString saveSettings(const QVariantMap &settings);
    // The servers of a group ("discovery" or "firewall") as rows {kind, address, isDefault,
    // browserOnly}; kind is "dht", "nostr", "mqtt", "peerjs", "stun" or "turn".
    Q_INVOKABLE QVariantList servers(const QString &group) const;
    // These change the server lists and save them for the next start; each returns what is
    // wrong, or "".
    Q_INVOKABLE QString addServer(const QString &kind, const QString &address);
    Q_INVOKABLE QString removeServer(const QString &group, int index);
    Q_INVOKABLE QString resetServers(const QString &group);
    // Checks every server in the saved lists again; results arrive in serverStatus.
    Q_INVOKABLE void checkServers();
    [[nodiscard]] QVariantList devices() const;
    // For a project or board you own: connected people without access, {key, name,
    // fingerprint}. They hold its invite; approving makes them members.
    Q_INVOKABLE QVariantList joinRequests(const QString &itemId) const;
    // Every request across projects and boards you own: {itemId, itemName, board, key,
    // name, fingerprint}.
    Q_INVOKABLE QVariantList allJoinRequests() const;

    // What you can do with a board now: "edit"; "offline" for a member with no owner
    // connected, who sees their last copy; "waiting" until an owner has let you in.
    Q_INVOKABLE QString boardAccess(const QString &boardId) const;

    // Tells peers which board this device shows and where the pointer is (world
    // coordinates); an empty boardId means no board.
    Q_INVOKABLE void setPresence(const QString &boardId, QPointF cursor, bool cursorVisible);
    // Tells peers which part of the open board is on this screen (world coordinates).
    Q_INVOKABLE void setPresenceView(const QString &boardId, const QRectF &view);
    // Other devices showing a board: {device, name, colorIndex, x, y, cursor, view}.
    Q_INVOKABLE QVariantList boardPresence(const QString &boardId) const;

    // The troubleshooting log: a summary of the current state, then the log itself. lastLines
    // limits the log to its most recent lines; 0 means all of it.
    Q_INVOKABLE QString logText(int lastLines = 0) const;
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void copyLog() const;
    // These return an error message, or an empty string when the log was saved.
    Q_INVOKABLE QString saveLog(const QUrl &file) const; // desktop and mobile
    Q_INVOKABLE QString downloadLog() const;             // browsers
    Q_INVOKABLE static QString logFileName();

  signals:
    void logChanged();
    void statusChanged();
    void peersChanged();
    void presenceChanged();
    void requestsChanged();
    void settingsChanged();
    void updateNeededChanged();
    void serverStatusChanged();
    // boardAccess may give a different answer now.
    void boardAccessChanged();
    // Someone without access reached a project or board you own.
    void joinRequested(const QString &name, const QString &itemName);

  private:
    // Applies a change to the saved server lists and saves them for the next start.
    QString changeServers(const std::function<void(ServerList &)> &change);

    enum class SpaceKind { Personal, Project, Board };
    struct Space {
        SpaceKind kind{SpaceKind::Personal};
        QUuid id;
        p2pdc::DiscoveryCapability capability{};
        std::unique_ptr<p2pdc::Subscription> watch;
        std::unique_ptr<p2pdc::Lease> lease;
        std::unique_ptr<p2pdc::IncomingListener> listener;
        bool published{false};
        bool caught{false};
        std::map<p2pdc::SigningPublicKey, p2pdc::Advertisement> advertisements;
    };
    struct SpaceSpec {
        SpaceKind kind{SpaceKind::Personal};
        QUuid id;
        QByteArray capability;
        QDateTime lastEdited;
    };
    struct RemoteUser {
        QByteArray userKey;
        QString name;
        int colorIndex{0};
    };
    struct Peer {
        QElapsedTimer started;
        QString route; // as last reported, for the troubleshooting summary
        p2pdc::RouteKind routeKind{p2pdc::RouteKind::Unknown};
        QString space;
        p2pdc::SigningPublicKey device{};
        std::uint64_t generation{0};
        std::unique_ptr<p2pdc::Connection> connection;
        bool ready{false};
        std::optional<RemoteUser> user;
        std::deque<QByteArray> outbox;
        MessageAssembler assembler;
        std::map<QUuid, Bytes> sentState; // what the peer is known to have
        std::set<QUuid> offeredStates;    // boards whose state it was told
        std::set<QUuid> sentBoards;
        std::set<QUuid> dirtyBoards;
        bool linkJoinSent{false};
        bool presenceDirty{false};
        bool presenceReliable{true}; // the next presence must not be lost
        std::optional<Presence> presence;
    };
    using PeerId = std::pair<QString, p2pdc::SigningPublicKey>;
    // A PDF coming from one peer, piece by piece.
    struct Download {
        PeerId from;
        QByteArray data;
        quint64 asked{0};    // offset up to which pieces were asked for
        quint64 total{0};    // the blob's size, once the first piece told it
        QElapsedTimer quiet; // since the last piece came, or the request went
    };

    // network.cpp: node, executor and spaces
    [[nodiscard]] p2pdc::Executor executor() const;
    [[nodiscard]] p2pdc::Scheduler scheduler() const;
    void setStatus(const QString &status);
    // Drops connections and asks the node to stop; false when there is nothing to wait for.
    bool beginShutdown(std::function<void()> stopped);
    void finishShutdown();
    void nodeStarted(p2pdc::NodeStartResult result);
    bool watch(const QString &key);
    bool announce(const QString &key);
    bool listen(const QString &key);
    void tryConnect(const QString &key);
    void request(const QString &key, p2pdc::IncomingRequestId id, p2pdc::Advertisement peer);
    [[nodiscard]] bool isOwnDevice(const p2pdc::SigningPublicKey &key) const;
    [[nodiscard]] bool isIncompatible(const p2pdc::SigningPublicKey &device);
    void markIncompatible(const p2pdc::SigningPublicKey &device, bool theyAreNewer);

    // network_spaces.cpp: which spaces are open
    [[nodiscard]] std::map<QString, SpaceSpec> wantedSpaces() const;
    [[nodiscard]] std::set<QString> essentialSpaces() const;
    void reconcileSpaces();
    void rotateSpaces();
    bool openSpace(const QString &key, SpaceKind kind, const QUuid &id,
                   const QByteArray &capability);
    void closeSpace(const QString &key);

    // network_boards.cpp: sending each peer what it lacks
    void offerBoardStates(Peer &peer);
    void handleBoardState(const PeerId &id, Peer &peer, const BoardState &state);
    bool queueNextBoard(const PeerId &id, Peer &peer);
    void offerLinkJoin(Peer &peer);
    void handleLinkJoin(const Peer &peer, const BoardLinkJoin &join);
    // network_blobs.cpp: fetching the PDFs that shared page boards show
    void requestMissingBlobs(const PeerId &id, Peer &peer, const QUuid &board);
    void handleBlobWant(Peer &peer, const BlobWant &want);
    void handleBlobChunk(const PeerId &id, Peer &peer, const BlobChunk &chunk);
    [[nodiscard]] bool blobSharedWith(const Peer &peer, const std::string &hash) const;
    void dropDownloadsFrom(const PeerId &id);
    // Asks another peer for the rest of a download; false when there is no one to ask.
    bool handOver(const std::string &hash, Download &download);
    void askAhead(Peer &peer, const std::string &hash, Download &download);
    void checkStalledDownloads();
    // An owner of the board, here or connected and sharing it.
    [[nodiscard]] bool boardLive(const QUuid &board) const;

    // network_discovery.cpp: watching, announcing and calls in
    void peerAdvertised(const QString &key, p2pdc::Advertisement value, p2pdc::PeerEvent event);
    [[nodiscard]] p2pdc::DiscoveryHandlers discoveryHandlers(const QString &key);
    [[nodiscard]] QString refusal(const Space &space, const PeerId &id,
                                  const p2pdc::Advertisement &advertisement);

    // network_log.cpp: the connection log
    void logConfiguration() const;
    [[nodiscard]] static QString spaceLabel(const QString &key);
    [[nodiscard]] QString summary() const;

    // network_peers.cpp: connections and sending
    void addPeer(const PeerId &id, std::uint64_t generation,
                 std::unique_ptr<p2pdc::Connection> connection);
    [[nodiscard]] QString peerLabel(const PeerId &id) const;
    [[nodiscard]] static p2pdc::ConnectOptions connectOptions();
    void connectPeer(const QString &space, const p2pdc::Advertisement &advertisement);
    [[nodiscard]] p2pdc::ConnectionHandlers handlers(PeerId id, std::uint64_t generation);
    void connectionState(const PeerId &id, std::uint64_t generation, p2pdc::ConnectionState state);
    void disconnectPeer(const PeerId &id, std::uint64_t generation);
    [[nodiscard]] Peer *livePeer(const PeerId &id, std::uint64_t generation);
    void queue(Peer &peer, const Message &message);
    void flush(const PeerId &id);
    bool sendOne(const PeerId &id, Peer &peer, const QByteArray &bytes);

    // network_instances.cpp: the other app instances on this device
    void setUpInstanceLink();
    void sendToInstances(const Message &message);
    void instanceJoined();
    void flushToInstances();
    void receiveFromInstance(const QByteArray &bytes);
    void applyInstanceBoard(const BoardUpdate &update);
    void applyInstancePersonal(const Personal &personal);

    // network_live.cpp: what may be lost, sent as datagrams
    void setUpLiveUpdates();
    void sendPresence(bool mustArrive = false);
    void flushPresence(const PeerId &id, Peer &peer);
    [[nodiscard]] Presence presenceFor(const Peer &peer) const;
    void applyPresence(Peer &peer, const Presence &presence);
    void receiveDatagram(const PeerId &id, std::uint64_t generation, const QByteArray &bytes);
    void draftUpdated(const QUuid &board, const Stroke &stroke, DraftState state);
    void sendLiveStroke();
    void handleLiveStroke(const Peer &peer, const LiveStroke &stroke);
    void applyLiveChange(const LiveStrokeTracker::Change &change);
    void settleLiveStrokes(const Peer &peer, const QUuid &board);
    void expireLiveStrokes();

    // network_sync.cpp: what flows between authenticated peers
    void receive(const PeerId &id, std::uint64_t generation, const QByteArray &bytes);
    void receivePart(const PeerId &id, Peer &peer, const MessagePart &part);
    void dispatch(const PeerId &id, Peer &peer, const std::optional<Message> &message);
    [[nodiscard]] bool peerHasAccess(const Peer &peer) const;
    bool handleHello(Peer &peer, const Hello &hello);
    void peerAuthenticated(const PeerId &id, Peer &peer);
    void offerAccess(Peer &peer);
    void handleAccess(const AccessChanges &message);
    void handlePersonal(const Peer &peer, const Personal &message);
    void handleBoardUpdate(const Peer &peer, const BoardUpdate &update);
    // Both sides may access the board and the connection's space covers it.
    [[nodiscard]] bool sharesBoard(const Peer &peer, const QUuid &board) const;
    // Board contents flow only between a board's owners and others: never member to member.
    [[nodiscard]] bool syncable(const Peer &peer, const QUuid &board) const;
    // Cursors and strokes being drawn, while the board is live.
    [[nodiscard]] bool sharesLiveActivity(const Peer &peer, const QUuid &board) const;
    [[nodiscard]] std::vector<QUuid> syncableBoards(const Peer &peer) const;
    void workspaceChanged();
    void boardChanged(const QUuid &board);
    void profileChanged();
    // The item a peer's space is about, if you own it and the peer has no access.
    [[nodiscard]] std::optional<QString> requestedItemName(const Peer &peer) const;

    Library &library_;
    NetworkConfig config_;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    std::unique_ptr<p2pdc::Node> node_;
    bool started_{false};
    bool stopping_{false};
    QString status_;
    std::map<QString, std::unique_ptr<Space>> spaces_;
    std::map<PeerId, std::unique_ptr<Peer>> peers_;
    std::uint64_t nextGeneration_{0};
    QTimer retryTimer_;
    QTimer rotationTimer_;
    std::uint64_t rotationStep_{0};
    QDeadlineTimer spaceOpenDelay_; // expired: spaces may be opened
    std::map<p2pdc::SigningPublicKey, QDeadlineTimer> incompatible_;
    bool updateNeeded_{false};
    InstanceLink *instanceLink_{nullptr};
    QTimer instanceFlushTimer_;
    std::set<QUuid> instanceDirtyBoards_;
    std::map<QUuid, Bytes> instanceSentState_;
    bool instancePersonalDirty_{false};
    QTimer presenceTimer_;
    QUuid presenceBoard_;
    QPointF cursor_;
    QRectF presenceView_;
    bool cursorVisible_{false};
    quint32 presenceSequence_{0};
    bool presenceMustArrive_{false};
    QTimer presenceSettleTimer_;
    QTimer liveStrokeTimer_;
    QTimer liveExpiryTimer_;
    LiveStrokeTracker liveStrokes_;
    std::optional<LiveStroke> outgoingStroke_;
    int outgoingRepeats_{0};
    quint32 nextLiveStroke_{0};
    bool drafting_{false};
    bool restartNeeded_{false};
    QTimer logTimer_;
    quint64 logRevision_{0};
    QTimer downloadTimer_;
    std::map<std::string, Download> downloads_; // by blob hash, one peer each
    ServerChecks *serverChecks_{nullptr};       // made on the first check
    QVariantMap serverStatus_;
};

} // namespace peerchalk::sync
