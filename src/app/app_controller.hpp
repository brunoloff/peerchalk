#pragma once

#include "library/instance_slot.hpp"
#include "sync/instance_link.hpp"
#include "ui/account_manager.hpp"
#include "ui/app_updater.hpp"

#include <QObject>
#include <QQmlApplicationEngine>

#include <memory>

namespace peerchalk {

// Shows the account screen or a signed-in account. Each signed-in session has its own QML
// engine, because the Library and Network singletons belong to one account; signing out
// stops the network and replaces the engine.
class AppController : public QObject {
    Q_OBJECT

  public:
    explicit AppController(InstanceSlot &slot, QObject *parent = nullptr);
    // boardLink: a board link the app was started with, if any.
    void start(const QString &boardLink = {});
    // Saves boards and closes connections before the application exits.
    void prepareToQuit();

  private:
    void mergeOtherInstances();
    void showAccountScreen();
    void showAccount(const QString &id);
    void signOut();
    void load(const char *type, const QString &accountDirectory);

    InstanceSlot &slot_;
    AccountManager accounts_;
    AppUpdater updater_;
    std::unique_ptr<sync::InstanceLink> instanceLink_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    bool inAccount_{false};
    bool signingOut_{false};
};

} // namespace peerchalk
