#include "repository/board_loader.hpp"

#include "board/board.hpp"
#include "library/codes.hpp"
#include "repository/encoding.hpp"
#include "repository/signed_content.hpp"

#include <algorithm>

namespace peerchalk::repository {
namespace {

const QString accessFolder = QStringLiteral("access/");
const QString snapshotsFolder = QStringLiteral("snapshots/");
const QString changesFolder = QStringLiteral("changes/");
const QString secretsFolder = QStringLiteral("secrets/");
const QString blobsFolder = QStringLiteral("blobs/");
const QString boardInfoFile = QStringLiteral("board.json");
constexpr qsizetype hashSize = 32;

void warn(LoadedBoard &board, const QString &path, const QString &reason) {
    board.warnings.push_back({path, reason});
}

// The name between folder and suffix, or "" when path is not folder<name>suffix.
QString nameIn(const QString &path, const QString &folder, const QString &suffix = {}) {
    if (!path.startsWith(folder) || !path.endsWith(suffix))
        return {};
    const QString name = path.mid(folder.size(), path.size() - folder.size() - suffix.size());
    return name.contains(QLatin1Char('/')) ? QString() : name;
}

void addOp(LoadedBoard &board, std::map<QByteArray, SignedOp> &ops, const QString &path,
           const SignedOp &op) {
    if (op.op.kind != ResourceKind::Board || op.op.resource != board.info.board)
        return warn(board, path, QStringLiteral("the entry is for another board"));
    const bool removal = op.op.action == AccessOp::Action::SetRole && op.op.role == Role::None;
    if (removal && op.op.snapshot.isEmpty())
        return warn(board, path,
                    QStringLiteral("a removal must name the snapshot taken before it"));
    ops.emplace(op.hash, op);
}

void loadAccess(LoadedBoard &board, const BoardFiles &files, const std::vector<QByteArray> &extra) {
    std::map<QByteArray, SignedOp> ops;
    for (const auto &[path, bytes] : files) {
        if (!path.startsWith(accessFolder))
            continue;
        try {
            const SignedOp op = decodeAccessFile(bytes);
            if (path != accessFolder + accessFileName(op))
                throw FileError("the file name is not the entry's hash");
            addOp(board, ops, path, op);
        } catch (const FileError &error) {
            warn(board, path, QString::fromStdString(error.what()));
        }
    }
    for (const QByteArray &bytes : extra) {
        try {
            addOp(board, ops, QStringLiteral("(history)"), decodeAccessFile(bytes));
        } catch (const FileError &error) {
            warn(board, QStringLiteral("(history)"), QString::fromStdString(error.what()));
        }
    }
    for (auto &entry : ops)
        board.ops.push_back(std::move(entry.second));
    board.access = replay(board.ops, ResourceKind::Board, board.info.board, board.info.creator,
                          [](const QUuid &, const QByteArray &, const QByteArray &) {
                              return Role::None; // boards are in no project here
                          });
}

// Checks a content file's place and signer; throws FileError when it does not belong.
SignedContent checkedContent(const LoadedBoard &board, const QString &path, const QByteArray &bytes,
                             ContentKind kind) {
    SignedContent content = openContent(bytes);
    const ContentHeader &header = content.header;
    if (header.board != board.info.board)
        throw FileError("the file is for another board");
    if (header.kind != kind)
        throw FileError("the file is in the wrong folder for its kind");
    if (kind == ContentKind::Snapshot && path != snapshotsFolder + toHex(sha256(bytes)) + ".yrs")
        throw FileError("the file name is not the snapshot's hash");
    if (kind == ContentKind::Changes &&
        path != changesFolder + toHex(header.certificate.deviceKey) + ".yrs")
        throw FileError("the change file is not named after its device");
    if (board.access.role(header.certificate.userKey) < Role::Member)
        throw FileError("its signer is not a member of the board");
    return content;
}

// A snapshot holds a whole document, so its own body says what it holds. A change file's
// body builds on snapshots, so its device's signed claim is used, for that device's sake.
StateVector contentState(const SignedContent &content, ContentKind kind) {
    Board probe;
    if (!probe.apply(byteView(content.body)))
        throw FileError("the content is not a Yrs update");
    const std::optional<StateVector> state =
        decodeStateVector(kind == ContentKind::Snapshot ? toByteArray(probe.stateVector())
                                                        : content.header.stateVector);
    if (!state)
        throw FileError("the content's state vector is malformed");
    return *state;
}

void loadContent(LoadedBoard &board, Board &document, const QString &path, const QByteArray &bytes,
                 ContentKind kind) {
    try {
        const SignedContent content = checkedContent(board, path, bytes, kind);
        const StateVector state = contentState(content, kind);
        document.apply(byteView(content.body));
        AcceptedContent accepted{path,
                                 sha256(bytes),
                                 content.header.certificate.deviceKey,
                                 content.header.certificate.userKey,
                                 state,
                                 content.header.covers,
                                 content.header.clients,
                                 content.body};
        (kind == ContentKind::Snapshot ? board.snapshots : board.changes)
            .push_back(std::move(accepted));
    } catch (const FileError &error) {
        warn(board, path, QString::fromStdString(error.what()));
    }
}

void loadSecret(LoadedBoard &board, const QString &path, const QByteArray &bytes) {
    try {
        const BoardSecret secret = decodeSecretFile(bytes);
        if (secret.board != board.info.board || path != secretsFolder + secretFileName(secret))
            throw FileError("the secret is not where it belongs");
        // Owners hand the secret to those they add; the creator is the admin, above them.
        const bool mayWrite = board.access.role(secret.signer) >= Role::Owner;
        if (!mayWrite)
            throw FileError("its signer may not hand out the board's secret");
        board.secrets[secret.epoch].push_back(secret);
    } catch (const FileError &error) {
        warn(board, path, QString::fromStdString(error.what()));
    }
}

void checkBlob(LoadedBoard &board, const QString &path, const QByteArray &bytes) {
    const QString name = nameIn(path, blobsFolder);
    if (fromHex(name, hashSize) != sha256(bytes))
        warn(board, path, QStringLiteral("the blob's name is not its hash"));
}

void loadFile(LoadedBoard &board, Board &document, const QString &path, const QByteArray &bytes) {
    if (path == boardInfoFile || path.startsWith(accessFolder))
        return;
    if (path.startsWith(snapshotsFolder))
        return loadContent(board, document, path, bytes, ContentKind::Snapshot);
    if (path.startsWith(changesFolder))
        return loadContent(board, document, path, bytes, ContentKind::Changes);
    if (path.startsWith(secretsFolder))
        return loadSecret(board, path, bytes);
    if (path.startsWith(blobsFolder))
        return checkBlob(board, path, bytes);
    warn(board, path, QStringLiteral("PeerChalk does not know this file"));
}

} // namespace

StateVector LoadedBoard::snapshotState() const {
    StateVector state;
    for (const AcceptedContent &snapshot : snapshots)
        state = merged(std::move(state), snapshot.state);
    return state;
}

int LoadedBoard::latestEpoch() const { return secrets.empty() ? -1 : secrets.rbegin()->first; }

LoadedBoard loadBoard(const BoardFiles &files, const std::vector<QByteArray> &extraAccess) {
    const auto info = files.find(boardInfoFile);
    if (info == files.end())
        throw FileError("the board has no board.json");
    LoadedBoard board;
    board.info = decodeBoardInfo(info->second);
    loadAccess(board, files, extraAccess);
    Board document;
    for (const auto &[path, bytes] : files)
        loadFile(board, document, path, bytes);
    board.state = toByteArray(document.encodeState());
    const std::string name = document.name();
    board.name = name.empty() ? board.access.name : QString::fromStdString(name);
    return board;
}

} // namespace peerchalk::repository
