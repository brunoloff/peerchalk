#pragma once

#include "library/backup.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>
#include <vector>

namespace peerchalk {

// One account on this device, as the account screen shows it.
struct AccountInfo {
    QString id;
    QString name;
    int colorIndex{0};
    QByteArray userKey;
    QDateTime lastOpened;
    QDateTime lastBackup; // invalid when never backed up
};

struct ImportedAccount {
    QString id;
    bool merged{false}; // the account was already here and gained what it lacked
};

// The accounts kept under `root`: accounts/<id>/ holds everything of one account (the
// files IdentityStore and Workspace use, plus account.json), and session.json names the
// signed-in account. New accounts are assembled in a hidden folder and renamed into place,
// so an interruption never leaves half an account. Accounts are local: nothing here
// contacts a server. Failures throw std::runtime_error or, for bad input,
// std::invalid_argument.
class AccountStore {
  public:
    explicit AccountStore(QString root);

    // Moves data from before accounts existed into a first account and signs it in.
    void migrateLegacyData();

    // Most recently opened first. Unreadable accounts are skipped with a warning.
    [[nodiscard]] std::vector<AccountInfo> accounts() const;
    [[nodiscard]] QString directory(const QString &id) const; // throws for unknown ids
    [[nodiscard]] QString create(const QString &name);
    // Creates this device's copy of an account from a transfer code.
    [[nodiscard]] QString link(const QString &transferCode);
    // A new account from a backup, or a merge into the same account if it is here.
    [[nodiscard]] ImportedAccount import(const BackupContents &backup);
    void remove(const QString &id);

    void markOpened(const QString &id);
    void markBackedUp(const QString &id);
    [[nodiscard]] QString current() const; // empty when signed out
    void setCurrent(const QString &id);

  private:
    [[nodiscard]] QString accountsDirectory() const;
    [[nodiscard]] QString stagingDirectory() const;
    [[nodiscard]] std::optional<AccountInfo> read(const QString &id) const;
    [[nodiscard]] std::optional<QString> findByKey(const QByteArray &userKey) const;
    [[nodiscard]] QString install(const QString &staging, const QByteArray &userKey);
    void updateMetadata(const QString &id, const char *field);

    QString root_;
};

} // namespace peerchalk
