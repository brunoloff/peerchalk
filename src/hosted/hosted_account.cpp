#include "hosted/hosted_account.hpp"

#include "library/files.hpp"
#include "library/identity_store.hpp"
#include "library/workspace.hpp"
#include "repository/encoding.hpp"

#include <QDir>

namespace peerchalk::hosted {
namespace {

constexpr int deviceFormatVersion = 1;

void writeDevice(const QString &directory, const OpenMessage &open) {
    writeJsonObject(QDir(directory).filePath(QStringLiteral("device.json")),
                    {{"signingSeed", QString::fromLatin1(open.deviceSeed.toBase64())},
                     {"recipientSecret", QString::fromLatin1(open.recipientSecret.toBase64())},
                     {"name", open.deviceName}},
                    deviceFormatVersion, true);
}

repository::LoadedBoard loaded(const OpenMessage &open) {
    try {
        repository::LoadedBoard board = repository::loadBoard(open.files, open.access);
        if (board.info.board != open.board)
            throw MessageError("the files are of another board");
        return board;
    } catch (const repository::FileError &error) {
        throw MessageError(error.what());
    }
}

} // namespace

repository::LoadedBoard prepareAccount(const QString &directory, const OpenMessage &open) {
    repository::LoadedBoard board = loaded(open);
    if (!QDir().mkpath(directory))
        throw std::runtime_error("Could not create " + directory.toStdString());
    writeDevice(directory, open);
    IdentityStore identity(directory, open.displayName);
    identity.adoptIdentity(open.transferCode);
    identity.setDisplayName(open.displayName);
    identity.setColorIndex(open.colorIndex);
    if (board.access.role(identity.signingKey()) < Role::Member)
        throw MessageError("you are not a member of this board");
    Workspace workspace(directory, identity.signingKey(), identity.identity().signer());
    workspace.join({Invite::Kind::Board, open.board, board.name, open.secret, board.info.creator});
    std::vector<std::pair<QByteArray, QByteArray>> ops;
    for (const SignedOp &op : board.ops)
        ops.emplace_back(op.payload, op.signature);
    workspace.acceptOps(ops);
    workspace.markRepositoryBoard(open.board);
    workspace.saveBoardState(open.board, board.state);
    return board;
}

} // namespace peerchalk::hosted
