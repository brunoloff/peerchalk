// Network: node lifecycle, executor and discovery spaces.
#include "sync/network.hpp"
#include "sync/connection_log.hpp"
#include "sync/instance_link.hpp"
#include "sync/p2pdc_names.hpp"

#include "ui/library.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QQmlEngine>
#include <QtLogging>

#include <algorithm>

namespace peerchalk::sync {

using namespace p2pdc;

namespace {

constexpr auto retryInterval = std::chrono::milliseconds(1500);
constexpr auto presenceInterval = std::chrono::milliseconds(100);
constexpr auto incompatibleRetry = std::chrono::minutes(5);

} // namespace

Network::Network(Library &library, NetworkConfig config, QObject *parent,
                 InstanceLink *instanceLink)
    : QObject(parent), library_(library), config_(std::move(config)), instanceLink_(instanceLink) {
    retryTimer_.setInterval(retryInterval);
    connect(&retryTimer_, &QTimer::timeout, this, [this] {
        reconcileSpaces();
        for (const auto &[key, space] : spaces_)
            tryConnect(key);
    });
    presenceTimer_.setSingleShot(true);
    presenceTimer_.setInterval(presenceInterval);
    connect(&presenceTimer_, &QTimer::timeout, this, [this] { sendPresence(); });
    rotationTimer_.setInterval(config_.spaceRotation);
    connect(&rotationTimer_, &QTimer::timeout, this, &Network::rotateSpaces);
    downloadTimer_.setInterval(std::chrono::seconds(5));
    connect(&downloadTimer_, &QTimer::timeout, this, &Network::checkStalledDownloads);
    logTimer_.setInterval(std::chrono::seconds(1));
    connect(&logTimer_, &QTimer::timeout, this, [this] {
        if (const quint64 revision = ConnectionLog::instance().revision(); revision != logRevision_) {
            logRevision_ = revision;
            emit logChanged();
        }
    });
    logTimer_.start();
    setUpLiveUpdates();
    setUpInstanceLink();
    connect(&library_, &Library::workspaceChanged, this, &Network::workspaceChanged);
    connect(&library_, &Library::boardChanged, this, &Network::boardChanged);
    connect(&library_, &Library::profileChanged, this, &Network::profileChanged);
    connect(this, &Network::peersChanged, this, &Network::requestsChanged);
    connect(this, &Network::peersChanged, this, &Network::boardAccessChanged);
    connect(&library_, &Library::workspaceChanged, this, &Network::boardAccessChanged);
    connect(&library_, &Library::workspaceChanged, this, &Network::requestsChanged);
    setStatus(config_.enabled ? tr("Not started") : tr("Offline: networking is turned off"));
}

Network::~Network() {
    // Without a shutdown, pending p2pdc callbacks are dropped rather than delivered.
    alive_->store(false);
    library_.setDraftListener({});
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
    auto *link = engine->property("instanceLink").value<InstanceLink *>();
    auto *network = new Network(*library, std::move(config), nullptr, link);
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
    logInfo(QStringLiteral("network"), QStringLiteral("Status: %1").arg(status));
    status_ = status;
    emit statusChanged();
}

void Network::start() {
    if (!config_.enabled || node_)
        return;
    DiscoveryConfig discovery;
    discovery.network_id = config_.networkId;
    discovery.bootstrap_servers = config_.bootstrap;
    discovery.browser_backends = config_.browserBackends;
    discovery.max_connections = 64;
    if (config_.loopbackOnly)
        discovery.bind_addresses = NativeBindAddresses{"127.0.0.1", "::1"};
    std::variant<std::unique_ptr<Node>, CryptoError, NodeIdentityError> created;
    library_.identityStore().identity().withDeviceSecrets(
        [&](std::span<const std::byte> seed, std::span<const std::byte> secret) {
            created = Node::create_with_identity(discovery, executor(), seed, secret);
        });
    logConfiguration();
    auto *node = std::get_if<std::unique_ptr<Node>>(&created);
    if (node == nullptr) {
        logError(QStringLiteral("network"), std::holds_alternative<CryptoError>(created)
                                                ? QStringLiteral("Could not create the node: crypto error")
                                                : QStringLiteral("Could not create the node: no identity support"));
        setStatus(tr("Offline: networking is unavailable in this build"));
        return;
    }
    node_ = std::move(*node);
    setStatus(tr("Connecting…"));
    if (!node_->start([this](NodeStartResult result) { nodeStarted(result); })) {
        logError(QStringLiteral("network"), QStringLiteral("The node refused to start"));
        setStatus(tr("Offline: the network could not start"));
    }
}

void Network::nodeStarted(NodeStartResult result) {
    const QString detail = describe(result.error);
    (result.status == NodeStartStatus::Started ? logInfo : logError)(
        QStringLiteral("network"), QStringLiteral("Node start: %1%2").arg(
            describe(result.status), detail.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(detail)));
    if (result.status != NodeStartStatus::Started) {
        setStatus(tr("Offline: the network could not start"));
        return;
    }
    started_ = true;
    reconcileSpaces();
    retryTimer_.start();
    rotationTimer_.start();
    setStatus(tr("Online"));
}

bool Network::beginShutdown(std::function<void()> stopped) {
    if (!node_ || stopping_)
        return false;
    stopping_ = true;
    logInfo(QStringLiteral("network"), QStringLiteral("Shutting down"));
    retryTimer_.stop();
    rotationTimer_.stop();
    peers_.clear();
    for (auto &[key, space] : spaces_) {
        space->listener.reset();
        space->watch.reset();
        space->lease.reset();
    }
    spaces_.clear();
    return node_->shutdown([stopped = std::move(stopped)](NodeStopResult) { stopped(); });
}

void Network::finishShutdown() {
    node_.reset();
    started_ = false;
    setStatus(tr("Offline"));
}

void Network::shutdown(std::chrono::milliseconds timeout) {
    if (!node_ || stopping_)
        return;
    QEventLoop loop;
    if (beginShutdown([&loop] { loop.quit(); })) {
        QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        loop.exec();
    }
    finishShutdown();
}

void Network::shutdownThen(std::chrono::milliseconds timeout, std::function<void()> done) {
    auto finished = std::make_shared<bool>(false);
    // Runs from the event loop: the node must not be destroyed inside its own callback.
    auto finish = [this, finished, done = std::move(done)] {
        if (std::exchange(*finished, true))
            return;
        finishShutdown();
        done();
    };
    if (beginShutdown([this, finish] { QTimer::singleShot(0, this, finish); }))
        QTimer::singleShot(timeout, this, finish);
    else
        QTimer::singleShot(0, this, finish);
}

bool Network::isIncompatible(const SigningPublicKey &device) {
    const auto found = incompatible_.find(device);
    if (found == incompatible_.end())
        return false;
    if (!found->second.hasExpired())
        return true;
    incompatible_.erase(found); // it may have been updated by now
    return false;
}

void Network::markIncompatible(const SigningPublicKey &device, bool theyAreNewer) {
    incompatible_[device] = QDeadlineTimer(incompatibleRetry);
    logWarning(QStringLiteral("connection"),
               QStringLiteral("Device %1 runs an incompatible PeerChalk version (%2); retrying in "
                              "5 minutes")
                   .arg(shortDevice(device),
                        theyAreNewer ? QStringLiteral("newer") : QStringLiteral("older")));
    if (theyAreNewer && !updateNeeded_) {
        updateNeeded_ = true;
        emit updateNeededChanged();
    }
}

bool Network::isOwnDevice(const SigningPublicKey &key) const {
    return node_ && node_->identity() && node_->identity()->signing_key == key;
}

} // namespace peerchalk::sync
