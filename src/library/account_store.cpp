#include "library/account_store.hpp"

#include "library/files.hpp"
#include "library/identity_store.hpp"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QUuid>
#include <QtLogging>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int formatVersion = 1;
constexpr qsizetype userKeySize = 32;
const QString migrationFolder = QStringLiteral(".migrating");
// Everything an installation kept in its data folder before accounts existed.
const std::array<QString, 6> legacyEntries = {
    QStringLiteral("device.json"),   QStringLiteral("profile.json"),
    QStringLiteral("contacts.json"), QStringLiteral("workspace.json"),
    QStringLiteral("network.json"),  QStringLiteral("boards")};

QString metadataPath(const QString &directory) {
    return QDir(directory).filePath(QStringLiteral("account.json"));
}

QString sessionPath(const QString &root) {
    return QDir(root).filePath(QStringLiteral("session.json"));
}

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

void moveEntry(const QDir &from, const QDir &to, const QString &name) {
    if (!from.exists(name))
        return;
    if (!QDir().rename(from.filePath(name), to.filePath(name)))
        throw std::runtime_error("Could not move " + from.filePath(name).toStdString());
}

// Removes a folder being assembled unless it was installed.
class StagingFolder {
  public:
    explicit StagingFolder(QString path) : path_(std::move(path)) {
        if (!QDir().mkpath(path_))
            throw std::runtime_error("Could not create " + path_.toStdString());
    }
    ~StagingFolder() { QDir(path_).removeRecursively(); }
    StagingFolder(const StagingFolder &) = delete;
    StagingFolder &operator=(const StagingFolder &) = delete;
    [[nodiscard]] const QString &path() const { return path_; }

  private:
    QString path_;
};

} // namespace

AccountStore::AccountStore(QString root) : root_(std::move(root)) {}

QString AccountStore::accountsDirectory() const {
    return QDir(root_).filePath(QStringLiteral("accounts"));
}

QString AccountStore::stagingDirectory() const {
    return QDir(accountsDirectory()).filePath(QStringLiteral(".new-") + newId());
}

void AccountStore::migrateLegacyData() {
    const QDir root(root_);
    const QDir staging(QDir(accountsDirectory()).filePath(migrationFolder));
    if (!root.exists(QStringLiteral("identity.json")) && !staging.exists())
        return;
    if (!QDir().mkpath(staging.path()))
        throw std::runtime_error("Could not create " + staging.path().toStdString());
    for (const QString &entry : legacyEntries)
        moveEntry(root, staging, entry);
    // Moved last: while it is still in place, an interrupted migration is resumed.
    moveEntry(root, staging, QStringLiteral("identity.json"));
    const IdentityStore identity(staging.path(), QStringLiteral("Me"));
    setCurrent(install(staging.path(), identity.signingKey()));
}

QString AccountStore::install(const QString &staging, const QByteArray &userKey) {
    writeJsonObject(metadataPath(staging),
                    {{QStringLiteral("userKey"), QString::fromLatin1(userKey.toBase64())},
                     {QStringLiteral("lastOpened"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}},
                    formatVersion, true);
    const QString id = newId();
    if (!QDir().rename(staging, QDir(accountsDirectory()).filePath(id)))
        throw std::runtime_error("Could not store the account");
    return id;
}

QString AccountStore::create(const QString &name) {
    const StagingFolder staging(stagingDirectory());
    IdentityStore identity(staging.path(), name);
    identity.setDisplayName(name);
    return install(staging.path(), identity.signingKey());
}

QString AccountStore::link(const QString &transferCode) {
    const StagingFolder staging(stagingDirectory());
    IdentityStore identity(staging.path(), QStringLiteral("Me"));
    identity.adoptIdentity(transferCode);
    if (findByKey(identity.signingKey()))
        throw std::invalid_argument("This account is already on this device");
    return install(staging.path(), identity.signingKey());
}

std::optional<AccountInfo> AccountStore::read(const QString &id) const {
    const QDir directory(QDir(accountsDirectory()).filePath(id));
    try {
        const auto metadata = readJsonObject(metadataPath(directory.path()), formatVersion);
        const auto profile =
            readJsonObject(directory.filePath(QStringLiteral("profile.json")), formatVersion);
        if (!metadata)
            return std::nullopt;
        AccountInfo info;
        info.id = id;
        info.userKey = base64Field(*metadata, QStringLiteral("userKey"), userKeySize);
        info.name = profile ? profile->value(QStringLiteral("name")).toString() : QString();
        info.colorIndex = profile ? profile->value(QStringLiteral("color")).toInt() : 0;
        info.lastOpened = QDateTime::fromString(
            metadata->value(QStringLiteral("lastOpened")).toString(), Qt::ISODateWithMs);
        info.lastBackup = QDateTime::fromString(
            metadata->value(QStringLiteral("lastBackup")).toString(), Qt::ISODateWithMs);
        return info;
    } catch (const std::exception &error) {
        qWarning("PeerChalk skips an unreadable account: %s", error.what());
        return std::nullopt;
    }
}

std::vector<AccountInfo> AccountStore::accounts() const {
    std::vector<AccountInfo> result;
    // Folders being assembled start with a dot, which this listing leaves out.
    for (const QString &id : QDir(accountsDirectory()).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (std::optional<AccountInfo> info = read(id))
            result.push_back(*std::move(info));
    std::ranges::sort(result, std::greater{}, &AccountInfo::lastOpened);
    return result;
}

std::optional<QString> AccountStore::findByKey(const QByteArray &userKey) const {
    for (const AccountInfo &account : accounts())
        if (account.userKey == userKey)
            return account.id;
    return std::nullopt;
}

QString AccountStore::directory(const QString &id) const {
    if (QUuid::fromString(id).isNull() || !read(id))
        throw std::invalid_argument("Unknown account");
    return QDir(accountsDirectory()).filePath(id);
}

void AccountStore::remove(const QString &id) {
    const QString path = directory(id);
    if (current() == id)
        setCurrent({});
    if (!QDir(path).removeRecursively())
        throw std::runtime_error("Could not remove the account's files");
}

void AccountStore::updateMetadata(const QString &id, const char *field) {
    const QString path = metadataPath(directory(id));
    std::optional<QJsonObject> metadata = readJsonObject(path, formatVersion);
    if (!metadata)
        throw std::runtime_error("The account's details are missing");
    metadata->insert(QString::fromLatin1(field),
                     QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    writeJsonObject(path, *metadata, formatVersion, true);
}

void AccountStore::adoptLastBackup(const QString &id, const QDateTime &when) {
    const std::optional<AccountInfo> info = read(id);
    if (!info || (info->lastBackup.isValid() && info->lastBackup >= when))
        return;
    const QString path = metadataPath(directory(id));
    std::optional<QJsonObject> metadata = readJsonObject(path, formatVersion);
    metadata->insert(QStringLiteral("lastBackup"), when.toString(Qt::ISODateWithMs));
    writeJsonObject(path, *metadata, formatVersion, true);
}

QByteArray AccountStore::keyOf(const QString &id) const {
    const std::optional<AccountInfo> info = read(id);
    return info ? info->userKey : QByteArray();
}

void AccountStore::markOpened(const QString &id) { updateMetadata(id, "lastOpened"); }

void AccountStore::markBackedUp(const QString &id) { updateMetadata(id, "lastBackup"); }

QString AccountStore::current() const {
    const auto session = readJsonObject(sessionPath(root_), formatVersion);
    const QString id = session ? session->value(QStringLiteral("current")).toString() : QString();
    return !id.isEmpty() && !QUuid::fromString(id).isNull() && read(id) ? id : QString();
}

void AccountStore::setCurrent(const QString &id) { updateSession(QStringLiteral("current"), id); }

bool AccountStore::opensLastAccount() const {
    const auto session = readJsonObject(sessionPath(root_), formatVersion);
    return session && session->value(QStringLiteral("openLastAccount")).toBool();
}

void AccountStore::setOpensLastAccount(bool opens) {
    updateSession(QStringLiteral("openLastAccount"), opens);
}

// session.json holds several settings; each change keeps the others.
void AccountStore::updateSession(const QString &key, const QJsonValue &value) {
    QJsonObject session = readJsonObject(sessionPath(root_), formatVersion).value_or(QJsonObject());
    session.insert(key, value);
    writeJsonObject(sessionPath(root_), session, formatVersion, true);
}

} // namespace peerchalk
