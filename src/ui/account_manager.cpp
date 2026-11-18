#include "ui/account_manager.hpp"

#include "library/codes.hpp"
#include "library/files.hpp"
#include "ui/library.hpp"
#include "ui/web_files.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QRegularExpression>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace peerchalk {
namespace {

BackupKind kindFromName(const QString &name) {
    if (name == QStringLiteral("everything"))
        return BackupKind::Everything;
    if (name == QStringLiteral("identity"))
        return BackupKind::Identity;
    if (name == QStringLiteral("people"))
        return BackupKind::People;
    throw std::invalid_argument("Unknown kind of backup");
}

QString fileNameFor(const QString &account, const QString &kind) {
    QString slug = account.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    const QString date = QDate::currentDate().toString(Qt::ISODate);
    return QStringLiteral("peerchalk-%1-%2-%3.peerchalk").arg(slug, kind, date);
}

} // namespace

AccountManager::AccountManager(const QString &root, QObject *parent)
    : QObject(parent), store_(root) {}

// The application supplies its manager through the engine; other engines get their own.
AccountManager *AccountManager::create(QQmlEngine *engine, QJSEngine *) {
    auto *shared = engine->property("accountManager").value<AccountManager *>();
    if (shared == nullptr)
        return new AccountManager(Library::defaultDataDirectory());
    QJSEngine::setObjectOwnership(shared, QJSEngine::CppOwnership);
    return shared;
}

bool AccountManager::attempt(const std::function<void()> &action) {
    try {
        action();
        return true;
    } catch (const std::exception &error) {
        emit errorOccurred(QString::fromStdString(error.what()));
        return false;
    }
}

QVariantList AccountManager::accounts() const {
    QVariantList result;
    for (const AccountInfo &account : store_.accounts())
        result.append(
            QVariantMap{{QStringLiteral("id"), account.id},
                        {QStringLiteral("name"), account.name},
                        {QStringLiteral("colorIndex"), account.colorIndex},
                        {QStringLiteral("fingerprint"), formatFingerprint(account.userKey)},
                        {QStringLiteral("lastBackup"), account.lastBackup}});
    return result;
}

QString AccountManager::currentAccountId() const { return store_.current(); }

QDateTime AccountManager::currentLastBackup() const {
    const QString id = store_.current();
    const std::vector<AccountInfo> all = store_.accounts();
    const auto it = std::ranges::find(all, id, &AccountInfo::id);
    return it == all.end() ? QDateTime() : it->lastBackup;
}

QString AccountManager::siteUrl() {
#ifdef __EMSCRIPTEN__
    return pageSiteUrl();
#else
    return qEnvironmentVariable("PEERCHALK_SITE_URL",
                                QStringLiteral("https://brunoloff.github.io/peerchalk"));
#endif
}

QString AccountManager::pendingBoardName() const {
    const std::optional<BoardLink> link = decodeBoardLink(pendingBoardLink_);
    return link ? link->name : QString();
}

void AccountManager::setPendingBoardLink(const QString &url) {
    pendingBoardLink_ = boardIdFromLink(url) ? url.trimmed() : QString();
    emit pendingBoardLinkChanged();
}

void AccountManager::readLinkFromPage() {
#ifdef __EMSCRIPTEN__
    setPendingBoardLink(pageUrl());
#endif
}

QString AccountManager::takePendingBoardLink() {
    QString link = std::exchange(pendingBoardLink_, {});
    if (!link.isEmpty()) {
#ifdef __EMSCRIPTEN__
        removeSecretFromPageUrl();
#endif
        emit pendingBoardLinkChanged();
    }
    return link;
}

QString AccountManager::createAccount(const QString &name) {
    QString id;
    if (attempt([&] { id = store_.create(name.trimmed()); }))
        emit accountsChanged();
    return id;
}

QString AccountManager::linkAccount(const QString &transferCode) {
    QString id;
    if (attempt([&] { id = store_.link(transferCode); }))
        emit accountsChanged();
    return id;
}

void AccountManager::openAccount(const QString &id) {
    if (attempt([&] {
            store_.markOpened(id);
            store_.setCurrent(id);
        }))
        emit openRequested(id);
}

void AccountManager::signOut() { emit signOutRequested(); }

void AccountManager::removeAccount(const QString &id) {
    if (otherInstancesRunning_ && otherInstancesRunning_()) {
        emit errorOccurred(tr("Close PeerChalk's other windows and tabs first."));
        return;
    }
    if (attempt([&] { store_.remove(id); }))
        emit accountsChanged();
}

bool AccountManager::prepareExport(const QString &id, const QString &kind,
                                   const QString &passphrase) {
    return attempt([&] {
        const BackupKind backupKind = kindFromName(kind);
        exportData_ = createBackup(store_.directory(id), backupKind, passphrase);
        const std::vector<AccountInfo> all = store_.accounts();
        const auto account = std::ranges::find(all, id, &AccountInfo::id);
        exportName_ = fileNameFor(account->name, kind);
        exportAccountId_ = id;
        exportCountsAsBackup_ = backupKind != BackupKind::People;
    });
}

void AccountManager::exportFinished() {
    exportData_.clear();
    if (exportCountsAsBackup_)
        store_.markBackedUp(exportAccountId_);
    emit accountsChanged();
    emit noticeOccurred(tr("Backup saved. Keep the file and its passphrase safe."));
}

void AccountManager::saveExport(const QUrl &file) {
    attempt([&] {
        writeFile(file.toLocalFile(), exportData_, true);
        exportFinished();
    });
}

void AccountManager::downloadExport() {
#ifdef __EMSCRIPTEN__
    attempt([&] {
        downloadFile(exportName_, exportData_);
        exportFinished();
    });
#else
    emit errorOccurred(tr("Downloads are only available in a browser"));
#endif
}

void AccountManager::loadImportFile(const QUrl &file) {
    if (attempt([&] {
            std::optional<QByteArray> contents = readFile(file.toLocalFile());
            if (!contents)
                throw std::invalid_argument("That file no longer exists");
            importData_ = *std::move(contents);
        }))
        emit importFileReady();
}

void AccountManager::pickImportFile() {
#ifdef __EMSCRIPTEN__
    pickFile([this](QByteArray contents) {
        importData_ = std::move(contents);
        emit importFileReady();
    });
#else
    emit errorOccurred(tr("This file picker is only available in a browser"));
#endif
}

QString AccountManager::importAccount(const QString &passphrase) {
    ImportedAccount imported;
    if (!attempt([&] { imported = store_.import(openBackup(importData_, passphrase)); }))
        return {};
    importData_.clear();
    emit accountsChanged();
    if (imported.merged)
        emit noticeOccurred(tr("This account was already here, so the backup was merged into it."));
    return imported.id;
}

QVariantMap AccountManager::importPeople(const QString &passphrase) {
    QVariantMap people;
    if (attempt([&] {
            const BackupContents backup = openBackup(importData_, passphrase);
            const auto file = backup.files.find(QStringLiteral("contacts.json"));
            if (file != backup.files.end())
                people = QJsonDocument::fromJson(file->second).object().toVariantMap();
        }))
        importData_.clear();
    return people;
}

} // namespace peerchalk
