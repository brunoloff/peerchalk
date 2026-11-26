#include "app/app_controller.hpp"

#include "sync/network.hpp"
#include "ui/host_link.hpp"
#include "ui/library.hpp"

#include <QCoreApplication>
#include <QQuickWindow>

#include <chrono>
#include <exception>
#include <memory>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace peerchalk {
namespace {

constexpr auto networkStopTimeout = std::chrono::seconds(3);

// Tells the page's loading screen (deploy/web/site/loader.js) that the app is on screen.
void reportShownOnce(QQuickWindow *window) {
#ifdef __EMSCRIPTEN__
    if (window == nullptr)
        return;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(window, &QQuickWindow::frameSwapped, window, [connection] {
        QObject::disconnect(*connection);
        emscripten_run_script("globalThis.peerchalkLoader && peerchalkLoader.ready()");
    });
#else
    Q_UNUSED(window)
#endif
}

QString hostedDirectory(const InstanceSlot &slot) {
    return slot.root() + QStringLiteral("/hosted");
}

QQuickWindow *rootWindow(QQmlApplicationEngine *engine) {
    if (engine == nullptr || engine->rootObjects().isEmpty())
        return nullptr;
    return qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
}

} // namespace

AppController::AppController(InstanceSlot &slot, QObject *parent)
    : QObject(parent), slot_(slot), accounts_(slot.root()),
      updater_(qEnvironmentVariable("APPIMAGE")) {
    accounts_.setOtherInstancesCheck([this] { return slot_.othersRunning(); });
    // Queued: these requests come from QML that the swap is about to destroy.
    connect(&accounts_, &AccountManager::openRequested, this, &AppController::showAccount,
            Qt::QueuedConnection);
    connect(&accounts_, &AccountManager::signOutRequested, this, &AppController::signOut,
            Qt::QueuedConnection);
}

void AppController::start(const QString &boardLink) {
    if (hosted::hostedPage()) {
        startHosted();
        return;
    }
    accounts_.store().migrateLegacyData();
    mergeOtherInstances();
    instanceLink_ = sync::InstanceLink::create(slot_);
    accounts_.setPendingBoardLink(boardLink);
    accounts_.readLinkFromPage();
    // Browsers always greet with the account screen while the page explains PeerChalk;
    // the installed app reopens the signed-in account when the person asked it to.
    const QString current = accounts_.store().current();
#ifdef __EMSCRIPTEN__
    const bool reopen = false;
#else
    const bool reopen = !current.isEmpty() && accounts_.store().opensLastAccount();
    if (accounts_.store().checksForUpdates())
        updater_.checkQuietly();
#endif
    if (reopen)
        showAccount(current);
    else
        showAccountScreen();
}

void AppController::startHosted() {
    hosted_ = std::make_unique<hosted::HostedSession>(hosted::pageBridge(), hostedDirectory(slot_),
                                                      hosted::HostedSession::Timing{});
    connect(hosted_.get(), &hosted::HostedSession::opened, this, &AppController::showHostedBoard);
    hosted_->start();
}

void AppController::showHostedBoard(const QUuid &board) {
    inAccount_ = true;
    load("Main", hostedDirectory(slot_));
    auto *library = engine_->singletonInstance<Library *>("PeerChalk", "Library");
    auto *network = engine_->singletonInstance<sync::Network *>("PeerChalk", "Network");
    hosted_->attach(*library, *network);
    auto *host = engine_->singletonInstance<HostLink *>("PeerChalk", "HostLink");
    host->setBackLabel(hosted_->backLabel());
    connect(host, &HostLink::backRequested, hosted_.get(), &hosted::HostedSession::requestBack);
    QMetaObject::invokeMethod(rootWindow(engine_.get()), "openBoard",
                              Q_ARG(QString, board.toString(QUuid::WithoutBraces)));
}

// A slot that could not be read is left in place, to be tried again next time.
void AppController::mergeOtherInstances() {
    std::vector<OtherInstance> merged;
    for (const OtherInstance &other : slot_.others()) {
        try {
            accounts_.store().mergeFrom(other.root, other.running);
            merged.push_back(other);
        } catch (const std::exception &error) {
            qWarning("PeerChalk could not read another instance's data: %s", error.what());
        }
    }
    slot_.finishMerge(merged);
}

void AppController::showAccountScreen() {
    inAccount_ = false;
    load("AccountsWindow", {});
}

void AppController::showAccount(const QString &id) {
    inAccount_ = true;
    load("Main", accounts_.store().directory(id));
}

// The new window opens before the old one closes, so the app never has no window, which
// would end it.
void AppController::load(const char *type, const QString &accountDirectory) {
    auto engine = std::make_unique<QQmlApplicationEngine>();
    engine->setProperty("accountManager", QVariant::fromValue(&accounts_));
    engine->setProperty("appUpdater", QVariant::fromValue(&updater_));
    engine->setProperty("accountDirectory", accountDirectory);
    engine->setProperty("instanceLink", QVariant::fromValue(instanceLink_.get()));
    engine->setProperty("hosted", hosted_ != nullptr);
    connect(
        engine.get(), &QQmlApplicationEngine::objectCreationFailed, this,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    const QQuickWindow *previous = rootWindow(engine_.get());
    if (previous != nullptr)
        engine->setInitialProperties({{QStringLiteral("x"), previous->x()},
                                      {QStringLiteral("y"), previous->y()},
                                      {QStringLiteral("width"), previous->width()},
                                      {QStringLiteral("height"), previous->height()}});
    engine->loadFromModule("PeerChalk", type);
    if (previous == nullptr)
        reportShownOnce(rootWindow(engine.get()));
    if (QQuickWindow *window = rootWindow(engine.get()); window != nullptr && previous != nullptr)
        window->setVisibility(previous->visibility());
    if (engine_)
        engine_.release()->deleteLater();
    engine_ = std::move(engine);
}

void AppController::signOut() {
    if (signingOut_ || !inAccount_)
        return;
    signingOut_ = true;
    engine_->singletonInstance<Library *>("PeerChalk", "Library")->saveBoards();
    auto *network = engine_->singletonInstance<sync::Network *>("PeerChalk", "Network");
    network->shutdownThen(networkStopTimeout, [this] {
        accounts_.store().setCurrent({});
        signingOut_ = false;
        showAccountScreen();
    });
}

void AppController::prepareToQuit() {
    if (!inAccount_ || !engine_)
        return;
    engine_->singletonInstance<Library *>("PeerChalk", "Library")->saveBoards();
    auto *network = engine_->singletonInstance<sync::Network *>("PeerChalk", "Network");
    if (network != nullptr)
        network->shutdown(networkStopTimeout);
}

} // namespace peerchalk
