#pragma once

#include "ui/account_manager.hpp"

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
    explicit AppController(const QString &dataRoot, QObject *parent = nullptr);
    // boardLink: a board link the app was started with, if any.
    void start(const QString &boardLink = {});
    // Saves boards and closes connections before the application exits.
    void prepareToQuit();

  private:
    void showAccountScreen();
    void showAccount(const QString &id);
    void signOut();
    void load(const char *type, const QString &accountDirectory);

    AccountManager accounts_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    bool inAccount_{false};
    bool signingOut_{false};
};

} // namespace peerchalk
