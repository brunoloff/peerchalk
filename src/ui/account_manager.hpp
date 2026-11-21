#pragma once

#include "library/account_store.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QQmlEngine;
class QJSEngine;

namespace peerchalk {

// The computer system a browser's user agent names: "Linux", "Windows", "macOS", or "" for
// phones, tablets and anything else.
[[nodiscard]] QString systemFromUserAgent(const QString &userAgent);

// The accounts on this device for QML: choosing, creating, linking, backing up and
// restoring them. Failures emit errorOccurred with a message for the user. Opening and
// signing out are carried out by the application, which swaps the screens.
class AccountManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Accounts)
    QML_SINGLETON
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QString currentAccountId READ currentAccountId NOTIFY accountsChanged)
    Q_PROPERTY(QDateTime currentLastBackup READ currentLastBackup NOTIFY accountsChanged)
    Q_PROPERTY(int minPassphraseLength READ minPassphraseLength CONSTANT)
    // Where this app is hosted, for the links it makes.
    Q_PROPERTY(QString siteUrl READ siteUrl CONSTANT)
    // The name of the board in the link the app was opened with; "" without one.
    Q_PROPERTY(QString pendingBoardName READ pendingBoardName NOTIFY pendingBoardLinkChanged)
    Q_PROPERTY(bool hasPendingBoardLink READ hasPendingBoardLink NOTIFY pendingBoardLinkChanged)
    // Whether the installed app reopens the signed-in account instead of the account screen.
    Q_PROPERTY(bool opensLastAccount READ opensLastAccount WRITE setOpensLastAccount NOTIFY
                   opensLastAccountChanged)
    Q_PROPERTY(bool checksForUpdates READ checksForUpdates WRITE setChecksForUpdates NOTIFY
                   checksForUpdatesChanged)
    // In browsers, the visitor's computer system ("Linux", "Windows" or "macOS") for the
    // download button; "" elsewhere, on phones and when unknown.
    Q_PROPERTY(QString visitorSystem READ visitorSystem CONSTANT)

  public:
    explicit AccountManager(const QString &root, QObject *parent = nullptr);
    static AccountManager *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    [[nodiscard]] AccountStore &store() { return store_; }
    [[nodiscard]] QVariantList accounts() const;
    [[nodiscard]] QString currentAccountId() const;
    [[nodiscard]] QDateTime currentLastBackup() const;
    [[nodiscard]] static int minPassphraseLength() {
        return static_cast<int>(minBackupPassphraseLength);
    }

    [[nodiscard]] static QString siteUrl();
    [[nodiscard]] bool opensLastAccount() const;
    void setOpensLastAccount(bool opens);
    [[nodiscard]] bool checksForUpdates() const;
    void setChecksForUpdates(bool checks);
    [[nodiscard]] static QString visitorSystem();
    [[nodiscard]] QString pendingBoardName() const;
    [[nodiscard]] bool hasPendingBoardLink() const { return !pendingBoardLink_.isEmpty(); }
    // Remembers a board link to open once an account is signed in.
    void setPendingBoardLink(const QString &url);
    // In browsers: picks up a link from the page's address.
    void readLinkFromPage();
    // Tells whether other app instances are running; accounts stay while they are.
    void setOtherInstancesCheck(std::function<bool()> check) {
        otherInstancesRunning_ = std::move(check);
    }
    // Hands over the link and removes its secret from the address bar.
    Q_INVOKABLE QString takePendingBoardLink();

    Q_INVOKABLE QString createAccount(const QString &name);
    Q_INVOKABLE QString linkAccount(const QString &transferCode);
    Q_INVOKABLE void openAccount(const QString &id);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void removeAccount(const QString &id);

    // Prepares an encrypted backup ("everything", "identity" or "people") to be saved.
    Q_INVOKABLE bool prepareExport(const QString &id, const QString &kind,
                                   const QString &passphrase);
    Q_INVOKABLE [[nodiscard]] QString exportFileName() const { return exportName_; }
    Q_INVOKABLE void saveExport(const QUrl &file); // desktop and mobile
    Q_INVOKABLE void downloadExport();             // browsers

    // Choosing a backup: importFileReady follows when one was read.
    Q_INVOKABLE void loadImportFile(const QUrl &file); // desktop and mobile
    Q_INVOKABLE void pickImportFile();                 // browsers; call during a click
    // Restores the chosen backup and returns the account's id, or "" on failure.
    Q_INVOKABLE QString importAccount(const QString &passphrase);
    // The known people in the chosen backup, for Library.importPeople.
    Q_INVOKABLE QVariantMap importPeople(const QString &passphrase);

  signals:
    void accountsChanged();
    void pendingBoardLinkChanged();
    void opensLastAccountChanged();
    void checksForUpdatesChanged();
    void errorOccurred(const QString &message);
    void noticeOccurred(const QString &message);
    void importFileReady();
    void openRequested(const QString &id);
    void signOutRequested();

  private:
    // Runs action, reporting a thrown error through errorOccurred; returns success.
    bool attempt(const std::function<void()> &action);
    void exportFinished();

    AccountStore store_;
    QByteArray exportData_;
    QString exportName_;
    QString exportAccountId_;
    bool exportCountsAsBackup_{false};
    QByteArray importData_;
    QString pendingBoardLink_;
    std::function<bool()> otherInstancesRunning_;
};

} // namespace peerchalk
