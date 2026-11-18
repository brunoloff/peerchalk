#include "library/backup.hpp"

#include "library/files.hpp"
#include "library/secrets.hpp"

#include <sodium.h>

#include <QCborMap>
#include <QCborValue>
#include <QDir>
#include <QIODevice>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <optional>

namespace peerchalk {
namespace {

const QByteArray magic = QByteArrayLiteral("PCHKBAK1");
constexpr quint32 writeOperations = 3;
constexpr quint32 writeMemoryKiB = 64 * 1024; // works in browsers as well as on desktops
constexpr quint32 maxOperations = 10;
constexpr quint32 minMemoryKiB = 8 * 1024;
constexpr quint32 maxMemoryKiB = 1024 * 1024;
constexpr qsizetype maxBackupBytes = qsizetype{1} << 30;
constexpr qsizetype saltSize = crypto_pwhash_SALTBYTES;
constexpr qsizetype nonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
constexpr qsizetype keySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
constexpr qsizetype tagSize = crypto_aead_xchacha20poly1305_ietf_ABYTES;
constexpr qsizetype headerSize = 8 + 4 + 4 + saltSize + nonceSize;
const std::array<QString, 3> identityFiles = {QStringLiteral("identity.json"),
                                              QStringLiteral("profile.json"),
                                              QStringLiteral("contacts.json")};

struct Header {
    quint32 operations;
    quint32 memoryKiB;
    QByteArray salt;
    QByteArray nonce;
};

const auto *bytes(const QByteArray &data) {
    return reinterpret_cast<const unsigned char *>(data.constData());
}
auto *bytes(QByteArray &data) { return reinterpret_cast<unsigned char *>(data.data()); }

void requireSodium() {
    if (sodium_init() < 0)
        throw std::runtime_error("The encryption library could not start");
}

QByteArray randomBytes(qsizetype size) {
    QByteArray result(size, '\0');
    randombytes_buf(result.data(), static_cast<std::size_t>(size));
    return result;
}

QByteArray encodeHeader(const Header &header) {
    QByteArray result = magic;
    result.resize(magic.size() + 8);
    qToLittleEndian(header.operations, result.data() + magic.size());
    qToLittleEndian(header.memoryKiB, result.data() + magic.size() + 4);
    return result + header.salt + header.nonce;
}

Header decodeHeader(const QByteArray &file) {
    if (file.size() < headerSize + tagSize || file.size() > maxBackupBytes ||
        !file.startsWith(magic))
        throw std::invalid_argument("This is not a PeerChalk backup");
    Header header{qFromLittleEndian<quint32>(file.constData() + magic.size()),
                  qFromLittleEndian<quint32>(file.constData() + magic.size() + 4),
                  file.mid(magic.size() + 8, saltSize),
                  file.mid(magic.size() + 8 + saltSize, nonceSize)};
    if (header.operations < 1 || header.operations > maxOperations ||
        header.memoryKiB < minMemoryKiB || header.memoryKiB > maxMemoryKiB)
        throw std::invalid_argument("This backup has unsupported settings");
    return header;
}

void deriveKey(const QString &passphrase, const Header &header, SecretBytes &key) {
    const SecretBytes secret{passphrase.toUtf8()};
    if (crypto_pwhash(bytes(key.bytes), static_cast<unsigned long long>(key.bytes.size()),
                      secret.bytes.constData(),
                      static_cast<unsigned long long>(secret.bytes.size()), bytes(header.salt),
                      header.operations, std::size_t{header.memoryKiB} * 1024,
                      crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw std::runtime_error("Not enough memory to process the passphrase");
}

QString kindName(BackupKind kind) {
    switch (kind) {
    case BackupKind::Everything:
        return QStringLiteral("everything");
    case BackupKind::Identity:
        return QStringLiteral("identity");
    case BackupKind::People:
        return QStringLiteral("people");
    }
    throw std::invalid_argument("Unknown kind of backup");
}

BackupKind kindFromName(const QString &name) {
    for (const BackupKind kind : {BackupKind::Everything, BackupKind::Identity, BackupKind::People})
        if (kindName(kind) == name)
            return kind;
    throw std::invalid_argument("This backup has an unknown kind");
}

bool isBoardFile(const QString &path) {
    if (!path.startsWith(QStringLiteral("boards/")) || !path.endsWith(QStringLiteral(".yrs")))
        return false;
    const QString id = path.mid(7, path.size() - 7 - 4);
    return id.size() == 36 && !QUuid::fromString(id).isNull();
}

// Backups name their own files, so only the expected names are ever written.
bool allowedFile(BackupKind kind, const QString &path) {
    if (kind == BackupKind::People)
        return path == QStringLiteral("contacts.json");
    if (std::ranges::find(identityFiles, path) != identityFiles.end())
        return true;
    return kind == BackupKind::Everything &&
           (path == QStringLiteral("workspace.json") || isBoardFile(path));
}

void addFile(QCborMap &files, const QDir &directory, const QString &path) {
    if (const std::optional<QByteArray> contents = readFile(directory.filePath(path)))
        files.insert(path, QCborValue(*contents));
}

QCborMap collectFiles(const QDir &directory, BackupKind kind) {
    QCborMap files;
    if (kind == BackupKind::People) {
        addFile(files, directory, QStringLiteral("contacts.json"));
        return files;
    }
    for (const QString &path : identityFiles)
        addFile(files, directory, path);
    if (kind == BackupKind::Everything) {
        addFile(files, directory, QStringLiteral("workspace.json"));
        const QDir boards(directory.filePath(QStringLiteral("boards")));
        for (const QString &name : boards.entryList({QStringLiteral("*.yrs")}, QDir::Files))
            addFile(files, directory, QStringLiteral("boards/") + name);
    }
    return files;
}

BackupContents parseContents(const QByteArray &plain) {
    const QCborMap root = QCborValue::fromCbor(plain).toMap();
    BackupContents contents;
    contents.kind = kindFromName(root[QStringLiteral("kind")].toString());
    contents.created =
        QDateTime::fromString(root[QStringLiteral("created")].toString(), Qt::ISODateWithMs);
    for (const auto &[key, value] : root[QStringLiteral("files")].toMap()) {
        const QString path = key.toString();
        if (!value.isByteArray() || !allowedFile(contents.kind, path))
            throw std::invalid_argument("This backup contains an unexpected file");
        contents.files.emplace(path, value.toByteArray());
    }
    if (contents.kind != BackupKind::People &&
        !contents.files.contains(QStringLiteral("identity.json")))
        throw std::invalid_argument("This backup has no identity");
    return contents;
}

} // namespace

BackupAccessError::BackupAccessError()
    : std::invalid_argument("Wrong passphrase, or the backup file is damaged") {}

QByteArray createBackup(const QString &accountDirectory, BackupKind kind,
                        const QString &passphrase) {
    if (passphrase.size() < minBackupPassphraseLength)
        throw std::invalid_argument("Use a passphrase of at least 12 characters");
    requireSodium();
    QCborMap root;
    root.insert(QStringLiteral("kind"), kindName(kind));
    root.insert(QStringLiteral("created"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("files"), collectFiles(QDir(accountDirectory), kind));
    const SecretBytes plain{root.toCborValue().toCbor()};
    const Header header{writeOperations, writeMemoryKiB, randomBytes(saltSize),
                        randomBytes(nonceSize)};
    const QByteArray encoded = encodeHeader(header);
    SecretBytes key{QByteArray(keySize, '\0')};
    deriveKey(passphrase, header, key);
    QByteArray sealed(plain.bytes.size() + tagSize, '\0');
    crypto_aead_xchacha20poly1305_ietf_encrypt(bytes(sealed), nullptr, bytes(plain.bytes),
                                               static_cast<unsigned long long>(plain.bytes.size()),
                                               bytes(encoded),
                                               static_cast<unsigned long long>(encoded.size()),
                                               nullptr, bytes(header.nonce), bytes(key.bytes));
    return encoded + sealed;
}

BackupContents openBackup(const QByteArray &file, const QString &passphrase) {
    requireSodium();
    const Header header = decodeHeader(file);
    const QByteArray encoded = file.first(headerSize);
    SecretBytes key{QByteArray(keySize, '\0')};
    deriveKey(passphrase, header, key);
    SecretBytes plain{QByteArray(file.size() - headerSize - tagSize, '\0')};
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            bytes(plain.bytes), nullptr, nullptr,
            reinterpret_cast<const unsigned char *>(file.constData()) + headerSize,
            static_cast<unsigned long long>(file.size() - headerSize), bytes(encoded),
            static_cast<unsigned long long>(encoded.size()), bytes(header.nonce),
            bytes(key.bytes)) != 0)
        throw BackupAccessError();
    return parseContents(plain.bytes);
}

void writeBackupFiles(const BackupContents &backup, const QString &directory) {
    const QDir target(directory);
    for (const auto &[path, contents] : backup.files)
        writeFile(target.filePath(path), contents, true);
}

} // namespace peerchalk
