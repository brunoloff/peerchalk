// AccountStore: bringing a backup onto this device.
#include "board/board.hpp"
#include "library/account_store.hpp"
#include "library/files.hpp"
#include "library/identity_store.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QDirIterator>
#include <QUuid>

#include <stdexcept>

namespace peerchalk {
namespace {

Bytes toBoardBytes(const QByteArray &data) {
    const auto *begin = reinterpret_cast<const std::byte *>(data.constData());
    return Bytes(begin, begin + data.size());
}

QByteArray fromBoardBytes(const Bytes &data) {
    return {reinterpret_cast<const char *>(data.data()), static_cast<qsizetype>(data.size())};
}

// Boards are CRDT documents, so both copies keep everything either of them has.
void mergeBoard(Workspace &target, const QUuid &id, const QByteArray &incoming) {
    const QByteArray existing = target.boardState(id);
    if (existing.isEmpty()) {
        target.saveBoardState(id, incoming);
        return;
    }
    Board board;
    if (!board.apply(toBoardBytes(existing)) || !board.apply(toBoardBytes(incoming)))
        throw std::runtime_error("A board in the backup is damaged");
    target.saveBoardState(id, fromBoardBytes(board.encodeState()));
}

void mergeAccount(const QString &sourceDirectory, const QString &targetDirectory) {
    const QString fallback = QStringLiteral("Me");
    IdentityStore sourceIdentity(sourceDirectory, fallback);
    IdentityStore targetIdentity(targetDirectory, fallback);
    targetIdentity.mergeContacts(sourceIdentity.exportContacts());
    Workspace source(sourceDirectory, sourceIdentity.signingKey(),
                     sourceIdentity.identity().signer());
    Workspace target(targetDirectory, targetIdentity.signingKey(),
                     targetIdentity.identity().signer());
    target.mergePersonal(source.exportPersonal());
    for (const BoardEntry &board : source.boards()) {
        const QByteArray state = source.boardState(board.id);
        if (!state.isEmpty() && target.hasBoard(board.id))
            mergeBoard(target, board.id, state);
    }
}

void copyAccountFiles(const QString &source, const QString &target) {
    QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString file = files.next();
        const QString relative = QDir(source).relativeFilePath(file);
        if (relative == QStringLiteral("device.json"))
            continue; // every instance has its own device keys
        const std::optional<QByteArray> contents = readFile(file);
        if (contents)
            writeFile(QDir(target).filePath(relative), *contents, true);
    }
}

} // namespace

void AccountStore::mergeFrom(const QString &otherRoot, bool otherIsRunning) {
    const AccountStore other(otherRoot);
    for (const AccountInfo &account : other.accounts()) {
        const QString source = other.directory(account.id);
        if (const std::optional<QString> mine = findByKey(account.userKey)) {
            mergeAccount(source, directory(*mine));
            if (account.lastBackup.isValid())
                adoptLastBackup(*mine, account.lastBackup);
        } else {
            const QString staging = stagingDirectory();
            copyAccountFiles(source, staging);
            if (!QDir().rename(staging, QDir(accountsDirectory()).filePath(account.id)))
                throw std::runtime_error("Could not copy an account from another instance");
        }
    }
    const QString theirs = other.current();
    if (theirs.isEmpty() || (!otherIsRunning && !current().isEmpty()))
        return;
    const QByteArray key = other.keyOf(theirs);
    if (const std::optional<QString> mine = findByKey(key))
        setCurrent(*mine);
}

ImportedAccount AccountStore::import(const BackupContents &backup) {
    if (backup.kind == BackupKind::People)
        throw std::invalid_argument(
            "This backup only holds known people. Import it from Settings → Account.");
    const QString staging = stagingDirectory();
    struct Cleanup {
        QString path;
        ~Cleanup() { QDir(path).removeRecursively(); }
    } cleanup{staging};
    writeBackupFiles(backup, staging);
    const IdentityStore identity(staging, QStringLiteral("Me"));
    // Opening it checks every signature and file before the account is accepted.
    const Workspace workspace(staging, identity.signingKey(), identity.identity().signer());
    if (const std::optional<QString> existing = findByKey(identity.signingKey())) {
        mergeAccount(staging, directory(*existing));
        return {*existing, true};
    }
    return {install(staging, identity.signingKey()), false};
}

} // namespace peerchalk
