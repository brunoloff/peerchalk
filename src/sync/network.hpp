#pragma once

#include "board/stroke_codec.hpp"
#include "sync/messages.hpp"
#include "sync/network_config.hpp"

#include <p2pdc/connection.h>
#include <p2pdc/discovery.h>
#include <p2pdc/incoming.h>
#include <p2pdc/node.h>

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QUuid>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <deque>
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

// Connects this device with others over p2pdc and keeps shared data in step.
//
// Each project, each board with its own invite, and the user's own devices form a
// "space": devices holding its capability find each other through it and connect (the
// lower device key dials). The first message on a connection is a Hello carrying the
// device's certificate, which proves the user it acts for. Peers in a space then
// exchange that space's signed manifests; board contents flow only for boards that both
// users may access according to those manifests, as Yrs updates relative to what was
// last sent. A user's own devices also exchange their workspace and contacts. Presence
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
    // People connected through an invite to a project or board you administer who are
    // not members yet, across all of them.
    Q_PROPERTY(int joinRequestCount READ joinRequestCount NOTIFY requestsChanged)

  public:
    Network(Library &library, NetworkConfig config, QObject *parent = nullptr);
    ~Network() override;
    // Starts networking for the Library singleton, configured from the environment.
    static Network *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    void start();
    // Closes every connection and stops the node, waiting up to `timeout` for p2pdc to
    // finish; call before the application exits.
    void shutdown(std::chrono::milliseconds timeout);

    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool online() const { return started_ && !stopping_; }
    [[nodiscard]] int connectedDevices() const;
    [[nodiscard]] int joinRequestCount() const;
    // For a project or board you administer: connected people who are not members,
    // {key, name, fingerprint}. They hold its invite; approving makes them members.
    Q_INVOKABLE QVariantList joinRequests(const QString &itemId) const;

    // Tells peers which board this device shows and where the pointer is (world
    // coordinates); an empty boardId means no board.
    Q_INVOKABLE void setPresence(const QString &boardId, QPointF cursor, bool cursorVisible);
    // Other devices showing a board: {device, name, colorIndex, x, y, cursor}.
    Q_INVOKABLE QVariantList boardPresence(const QString &boardId) const;

  signals:
    void statusChanged();
    void peersChanged();
    void presenceChanged();
    void requestsChanged();
    // Someone not yet a member reached a project or board you administer.
    void joinRequested(const QString &name, const QString &itemName);

  private:
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
    struct RemoteUser {
        QByteArray userKey;
        QString name;
        int colorIndex{0};
    };
    struct Peer {
        QString space;
        p2pdc::SigningPublicKey device{};
        std::uint64_t generation{0};
        std::unique_ptr<p2pdc::Connection> connection;
        bool ready{false};
        std::optional<RemoteUser> user;
        std::deque<QByteArray> outbox;
        std::map<QUuid, Bytes> sentState;
        std::set<QUuid> dirtyBoards;
        bool presenceDirty{false};
        std::optional<Presence> presence;
    };
    using PeerId = std::pair<QString, p2pdc::SigningPublicKey>;

    // network.cpp: node, executor and spaces
    [[nodiscard]] p2pdc::Executor executor() const;
    [[nodiscard]] p2pdc::Scheduler scheduler() const;
    void setStatus(const QString &status);
    void nodeStarted(p2pdc::NodeStartResult result);
    void reconcileSpaces();
    void openSpace(const QString &key, SpaceKind kind, const QUuid &id,
                   const QByteArray &capability);
    void closeSpace(const QString &key);
    bool watch(const QString &key);
    bool announce(const QString &key);
    bool listen(const QString &key);
    void tryConnect(const QString &key);
    void request(const QString &key, p2pdc::IncomingRequestId id, p2pdc::Advertisement peer);
    [[nodiscard]] bool isOwnDevice(const p2pdc::SigningPublicKey &key) const;

    // network_peers.cpp: connections and sending
    [[nodiscard]] static p2pdc::ConnectOptions connectOptions();
    void connectPeer(const QString &space, const p2pdc::Advertisement &advertisement);
    [[nodiscard]] p2pdc::ConnectionHandlers handlers(PeerId id, std::uint64_t generation);
    void connectionState(const PeerId &id, std::uint64_t generation, p2pdc::ConnectionState state);
    void disconnectPeer(const PeerId &id, std::uint64_t generation);
    [[nodiscard]] Peer *livePeer(const PeerId &id, std::uint64_t generation);
    void queue(Peer &peer, const Message &message);
    void flush(const PeerId &id);
    bool flushBoards(const PeerId &id, Peer &peer);
    bool sendOne(const PeerId &id, Peer &peer, const QByteArray &bytes);

    // network_sync.cpp: what flows between authenticated peers
    void receive(const PeerId &id, std::uint64_t generation, const QByteArray &bytes);
    bool handleHello(Peer &peer, const Hello &hello);
    void peerAuthenticated(const PeerId &id, Peer &peer);
    void offerManifests(Peer &peer);
    void handleManifests(const Manifests &message);
    void handlePersonal(const Peer &peer, const Personal &message);
    void handleBoardUpdate(const Peer &peer, const BoardUpdate &update);
    [[nodiscard]] bool syncable(const Peer &peer, const QUuid &board) const;
    [[nodiscard]] std::vector<QUuid> syncableBoards(const Peer &peer) const;
    void workspaceChanged();
    void boardChanged(const QUuid &board);
    void sendPresence();
    [[nodiscard]] Presence presenceFor(const Peer &peer) const;
    // The item a peer's space is about, if you administer it and the peer is not in it.
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
    QTimer presenceTimer_;
    QUuid presenceBoard_;
    QPointF cursor_;
    bool cursorVisible_{false};
};

} // namespace peerchalk::sync
