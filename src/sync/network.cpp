// Network: node lifecycle, executor and discovery spaces.
#include "sync/network.hpp"

#include "ui/library.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QQmlEngine>
#include <QtLogging>

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr std::size_t maxSpaces = 16; // p2pdc's limit on subscriptions per node
constexpr auto retryInterval = std::chrono::milliseconds(1500);
constexpr auto presenceInterval = std::chrono::milliseconds(100);

DiscoveryCapability toCapability(const QByteArray &bytes) {
    DiscoveryCapability capability{};
    std::copy_n(reinterpret_cast<const std::byte *>(bytes.constData()), capability.size(),
                capability.begin());
    return capability;
}

} // namespace

Network::Network(Library &library, NetworkConfig config, QObject *parent)
    : QObject(parent), library_(library), config_(std::move(config)) {
    retryTimer_.setInterval(retryInterval);
    connect(&retryTimer_, &QTimer::timeout, this, [this] {
        reconcileSpaces();
        for (const auto &[key, space] : spaces_)
            tryConnect(key);
    });
    presenceTimer_.setSingleShot(true);
    presenceTimer_.setInterval(presenceInterval);
    connect(&presenceTimer_, &QTimer::timeout, this, &Network::sendPresence);
    connect(&library_, &Library::workspaceChanged, this, &Network::workspaceChanged);
    connect(&library_, &Library::boardChanged, this, &Network::boardChanged);
    connect(&library_, &Library::profileChanged, this, &Network::profileChanged);
    connect(this, &Network::peersChanged, this, &Network::requestsChanged);
    connect(&library_, &Library::workspaceChanged, this, &Network::requestsChanged);
    setStatus(config_.enabled ? tr("Not started") : tr("Offline: networking is turned off"));
}

Network::~Network() {
    // Without a shutdown, pending p2pdc callbacks are dropped rather than delivered.
    alive_->store(false);
}

Network *Network::create(QQmlEngine *engine, QJSEngine *) {
    auto *library = engine->singletonInstance<Library *>("PeerChalk", "Library");
    NetworkConfig config;
    try {
        config = NetworkConfig::from(NetworkSettings::load(library->directory()).withEnvironment());
    } catch (const std::exception &error) {
        qWarning("PeerChalk networking is off: %s", error.what());
        config.enabled = false;
    }
    auto *network = new Network(*library, std::move(config));
    network->start();
    return network;
}

// Every task runs later on the Qt main thread, never inline, as p2pdc requires.
Executor Network::executor() const {
    auto alive = alive_;
    return [alive](std::function<void()> task) {
        auto *application = QCoreApplication::instance();
        if (application == nullptr || !alive->load())
            return;
        QMetaObject::invokeMethod(
            application,
            [alive, task = std::move(task)] {
                if (alive->load())
                    task();
            },
            Qt::QueuedConnection);
    };
}

// p2pdc may call the scheduler from its own threads, so timers are created on the main
// thread first.
Scheduler Network::scheduler() const {
    auto alive = alive_;
    return [alive](std::chrono::milliseconds delay, std::function<void()> task) -> CancelTimer {
        auto canceled = std::make_shared<std::atomic_bool>(false);
        auto *application = QCoreApplication::instance();
        if (application == nullptr)
            throw std::runtime_error("no application to schedule on");
        QMetaObject::invokeMethod(
            application,
            [alive, canceled, delay, application, task = std::move(task)]() mutable {
                if (!alive->load() || canceled->load())
                    return;
                QTimer::singleShot(delay, application, [alive, canceled, task = std::move(task)] {
                    if (alive->load() && !canceled->load())
                        task();
                });
            },
            Qt::QueuedConnection);
        return [canceled] { canceled->store(true); };
    };
}

void Network::setStatus(const QString &status) {
    if (status == status_)
        return;
    status_ = status;
    emit statusChanged();
}

void Network::start() {
    if (!config_.enabled || node_)
        return;
    DiscoveryConfig discovery;
    discovery.network_id = config_.networkId;
    discovery.bootstrap_servers = config_.bootstrap;
    discovery.max_connections = 64;
    if (config_.loopbackOnly)
        discovery.bind_addresses = NativeBindAddresses{"127.0.0.1", "::1"};
    std::variant<std::unique_ptr<Node>, CryptoError, NodeIdentityError> created;
    library_.identityStore().identity().withDeviceSecrets(
        [&](std::span<const std::byte> seed, std::span<const std::byte> secret) {
            created = Node::create_with_identity(discovery, executor(), seed, secret);
        });
    auto *node = std::get_if<std::unique_ptr<Node>>(&created);
    if (node == nullptr) {
        setStatus(tr("Offline: networking is unavailable in this build"));
        return;
    }
    node_ = std::move(*node);
    setStatus(tr("Connecting…"));
    if (!node_->start([this](NodeStartResult result) { nodeStarted(result); }))
        setStatus(tr("Offline: the network could not start"));
}

void Network::nodeStarted(NodeStartResult result) {
    if (result.status != NodeStartStatus::Started) {
        setStatus(tr("Offline: the network could not start"));
        return;
    }
    started_ = true;
    reconcileSpaces();
    retryTimer_.start();
    setStatus(tr("Online"));
}

void Network::shutdown(std::chrono::milliseconds timeout) {
    if (!node_ || stopping_)
        return;
    stopping_ = true;
    retryTimer_.stop();
    peers_.clear();
    for (auto &[key, space] : spaces_) {
        space->listener.reset();
        space->watch.reset();
        space->lease.reset();
    }
    spaces_.clear();
    QEventLoop loop;
    const bool begun = node_->shutdown([&loop](NodeStopResult) { loop.quit(); });
    if (begun) {
        QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        loop.exec();
    }
    node_.reset();
    started_ = false;
    setStatus(tr("Offline"));
}

bool Network::isOwnDevice(const SigningPublicKey &key) const {
    return node_ && node_->identity() && node_->identity()->signing_key == key;
}

// Opens a space for every project and board this device can find peers for, and closes
// spaces that are no longer needed.
void Network::reconcileSpaces() {
    if (!started_ || stopping_)
        return;
    std::map<QString, std::tuple<SpaceKind, QUuid, QByteArray>> wanted;
    const Workspace &workspace = library_.workspace();
    wanted[QStringLiteral("personal")] = {SpaceKind::Personal, QUuid(),
                                          library_.identityStore().identity().personalCapability()};
    for (const ProjectEntry &project : workspace.projects())
        if (!project.deleted())
            wanted[QStringLiteral("p:") + project.id.toString(QUuid::WithoutBraces)] = {
                SpaceKind::Project, project.id, project.capability};
    for (const BoardEntry &board : workspace.boards())
        if (!board.deleted() && board.capability.size() == 32)
            wanted[QStringLiteral("b:") + board.id.toString(QUuid::WithoutBraces)] = {
                SpaceKind::Board, board.id, board.capability};
    std::vector<QString> obsolete;
    for (const auto &[key, space] : spaces_)
        if (!wanted.contains(key))
            obsolete.push_back(key);
    for (const QString &key : obsolete)
        closeSpace(key);
    for (const auto &[key, spec] : wanted) {
        if (spaces_.contains(key))
            continue;
        if (spaces_.size() >= maxSpaces) {
            setStatus(
                tr("Online, but only %1 projects and boards can be shared at once").arg(maxSpaces));
            break;
        }
        openSpace(key, std::get<0>(spec), std::get<1>(spec), std::get<2>(spec));
    }
}

void Network::openSpace(const QString &key, SpaceKind kind, const QUuid &id,
                        const QByteArray &capability) {
    auto space = std::make_unique<Space>();
    space->kind = kind;
    space->id = id;
    space->capability = toCapability(capability);
    spaces_[key] = std::move(space);
    if (!watch(key) || !announce(key))
        spaces_.erase(key); // retried by the next reconcile
}

void Network::closeSpace(const QString &key) {
    std::erase_if(peers_, [&key](const auto &entry) { return entry.first.first == key; });
    spaces_.erase(key);
    emit peersChanged();
    emit presenceChanged();
}

bool Network::watch(const QString &key) {
    Space &space = *spaces_.at(key);
    auto result =
        node_->watch(space.capability, scheduler(), {},
                     {[this, key](Advertisement value, PeerEvent event) {
                          const auto it = spaces_.find(key);
                          if (it == spaces_.end() || isOwnDevice(value.signing_public_key))
                              return;
                          auto &known = it->second->advertisements;
                          if (event == PeerEvent::Expired) {
                              const auto found = known.find(value.signing_public_key);
                              if (found != known.end() &&
                                  found->second.fields.instance_id == value.fields.instance_id)
                                  known.erase(found);
                              return;
                          }
                          known.insert_or_assign(value.signing_public_key, std::move(value));
                          tryConnect(key);
                      },
                      [this, key](bool ok) {
                          if (const auto it = spaces_.find(key); it != spaces_.end() && ok) {
                              it->second->caught = true;
                              tryConnect(key);
                          }
                      },
                      [this, key](WatchError) {
                          // Reopened by the next reconcile.
                          QTimer::singleShot(0, this, [this, key] { closeSpace(key); });
                      }});
    auto *subscription = std::get_if<std::unique_ptr<Subscription>>(&result);
    if (subscription == nullptr)
        return false;
    space.watch = std::move(*subscription);
    return true;
}

bool Network::announce(const QString &key) {
    Space &space = *spaces_.at(key);
    auto result =
        node_->announce(space.capability, scheduler(), {},
                        {[this, key] {
                             if (const auto it = spaces_.find(key); it != spaces_.end()) {
                                 it->second->published = true;
                                 tryConnect(key);
                             }
                         },
                         [this, key](AnnouncementError error) {
                             if (error != AnnouncementError::Retrying)
                                 QTimer::singleShot(0, this, [this, key] { closeSpace(key); });
                         }});
    auto *lease = std::get_if<std::unique_ptr<Lease>>(&result);
    if (lease == nullptr)
        return false;
    space.lease = std::move(*lease);
    return true;
}

bool Network::listen(const QString &key) {
    Space &space = *spaces_.at(key);
    IncomingOptions options;
    options.persistent = true;
    IncomingHandlers callbacks;
    callbacks.on_error = [this, key](IncomingError) {
        QTimer::singleShot(0, this, [this, key] {
            if (const auto it = spaces_.find(key); it != spaces_.end())
                it->second->listener.reset(); // recreated by the next tryConnect
        });
    };
    callbacks.on_request_identified = [this, key](IncomingRequestId id, Advertisement peer) {
        request(key, id, std::move(peer));
    };
    callbacks.on_overflow = [] {};
    auto result =
        node_->listen(*space.lease, *space.watch, scheduler(), options, std::move(callbacks));
    auto *listener = std::get_if<std::unique_ptr<IncomingListener>>(&result);
    if (listener == nullptr)
        return false;
    space.listener = std::move(*listener);
    return true;
}

// The device with the lower key dials; the other accepts.
void Network::tryConnect(const QString &key) {
    const auto it = spaces_.find(key);
    if (it == spaces_.end() || stopping_ || !node_ || !node_->identity())
        return;
    Space &space = *it->second;
    if (!space.published || !space.caught || !space.watch || !space.lease)
        return;
    if (!space.listener && !listen(key))
        return;
    const SigningPublicKey own = node_->identity()->signing_key;
    for (const auto &[device, advertisement] : space.advertisements) {
        const auto peer = peers_.find({key, device});
        const bool connected = peer != peers_.end() && peer->second->connection;
        if (own < device && !connected && space.watch->is_current(advertisement))
            connectPeer(key, advertisement);
    }
}

void Network::request(const QString &key, IncomingRequestId id, Advertisement advertisement) {
    const auto it = spaces_.find(key);
    if (it == spaces_.end())
        return;
    Space &space = *it->second;
    const SigningPublicKey device = advertisement.signing_public_key;
    const auto existing = peers_.find({key, device});
    const bool busy = existing != peers_.end() && existing->second->connection;
    if (!node_->identity() || !space.watch->is_current(advertisement) ||
        device >= node_->identity()->signing_key || busy) {
        static_cast<void>(space.listener->reject(id));
        return;
    }
    const auto generation = ++nextGeneration_;
    auto result = node_->accept(*space.listener, id, config_.ice, scheduler(), connectOptions(),
                                handlers({key, device}, generation));
    auto *connection = std::get_if<std::unique_ptr<Connection>>(&result);
    if (connection == nullptr) {
        static_cast<void>(space.listener->reject(id));
        return;
    }
    auto peer = std::make_unique<Peer>();
    peer->space = key;
    peer->device = device;
    peer->generation = generation;
    peer->connection = std::move(*connection);
    peers_[{key, device}] = std::move(peer);
}

} // namespace peerchalk::sync
