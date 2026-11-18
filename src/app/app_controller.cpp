#include "app/app_controller.hpp"

#include "sync/network.hpp"
#include "ui/library.hpp"

#include <QCoreApplication>
#include <QQuickWindow>

#include <chrono>

namespace peerchalk {
namespace {

constexpr auto networkStopTimeout = std::chrono::seconds(3);

QQuickWindow *rootWindow(QQmlApplicationEngine *engine) {
    if (engine == nullptr || engine->rootObjects().isEmpty())
        return nullptr;
    return qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
}

} // namespace

AppController::AppController(const QString &dataRoot, QObject *parent)
    : QObject(parent), accounts_(dataRoot) {
    // Queued: these requests come from QML that the swap is about to destroy.
    connect(&accounts_, &AccountManager::openRequested, this, &AppController::showAccount,
            Qt::QueuedConnection);
    connect(&accounts_, &AccountManager::signOutRequested, this, &AppController::signOut,
            Qt::QueuedConnection);
}

void AppController::start(const QString &boardLink) {
    accounts_.store().migrateLegacyData();
    accounts_.setPendingBoardLink(boardLink);
    accounts_.readLinkFromPage();
    const QString current = accounts_.store().current();
    if (current.isEmpty())
        showAccountScreen();
    else
        showAccount(current);
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
    engine->setProperty("accountDirectory", accountDirectory);
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
