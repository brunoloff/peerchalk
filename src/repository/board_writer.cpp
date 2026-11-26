#include "repository/board_writer.hpp"

#include "library/secrets.hpp"
#include "repository/encoding.hpp"
#include "repository/signed_content.hpp"

#include <algorithm>
#include <stdexcept>

namespace peerchalk::repository {
namespace {

constexpr qsizetype maxCapabilityBytes = 64;

QString snapshotPath(const QByteArray &hash) {
    return QStringLiteral("snapshots/") + toHex(hash) + QStringLiteral(".yrs");
}

std::vector<QByteArray> snapshotHashes(const LoadedBoard &loaded) {
    std::vector<QByteArray> hashes;
    for (const AcceptedContent &snapshot : loaded.snapshots)
        hashes.push_back(snapshot.hash);
    return hashes;
}

QByteArray sealedContent(const Board &document, const QUuid &board, ContentKind kind,
                         const QByteArray &body, const Author &author,
                         std::vector<QByteArray> covers,
                         const std::vector<std::uint64_t> &clients) {
    const ContentHeader header{board,
                               kind,
                               author.certificate,
                               std::move(covers),
                               clients,
                               toByteArray(document.stateVector())};
    return sealContent(header, body, author.device);
}

void addSecret(FileChanges &files, const QUuid &board, int epoch, const QByteArray &user,
               const QByteArray &capability, const Author &author) {
    const BoardSecret secret{board, epoch, user, sealCapability(user, capability), author.userKey};
    files[QStringLiteral("secrets/") + secretFileName(secret)] =
        encodeSecretFile(secret, author.user);
}

// Signs op as the author's next change, and checks that it applies.
SignedOp signedEntry(const LoadedBoard &loaded, const Author &author, AccessOp op) {
    op.kind = ResourceKind::Board;
    op.resource = loaded.info.board;
    op.clock = loaded.access.clock + 1;
    op.signer = author.userKey;
    SignedOp entry = signOp(op, author.user);
    std::vector<SignedOp> ops = loaded.ops;
    ops.push_back(entry);
    const AccessState after =
        replay(ops, ResourceKind::Board, loaded.info.board, loaded.info.creator,
               [](const QUuid &, const QByteArray &, const QByteArray &) { return Role::None; });
    if (after.role(op.user) != op.role)
        throw std::invalid_argument("Your role on this board does not allow that change");
    return entry;
}

} // namespace

Author authorOf(const UserIdentity &identity) {
    return {identity.userKey(), identity.deviceCertificate(), identity.signer(),
            identity.deviceSigner()};
}

QString changeFilePath(const QByteArray &device) {
    return QStringLiteral("changes/") + toHex(device) + QStringLiteral(".yrs");
}

NewBoard createBoard(const QString &name, const QString &displayName, const Author &author) {
    if (name.trimmed().isEmpty())
        throw std::invalid_argument("A board needs a name");
    NewBoard created{QUuid::createUuid(), newCapability(), {}};
    created.files[QStringLiteral("board.json")] = encodeBoardInfo({created.id, author.userKey});
    AccessOp create;
    create.kind = ResourceKind::Board;
    create.resource = created.id;
    create.clock = 1;
    create.signer = author.userKey;
    create.action = AccessOp::Action::Create;
    create.name = name.trimmed();
    create.signerName = displayName;
    const SignedOp entry = signOp(create, author.user);
    created.files[QStringLiteral("access/") + accessFileName(entry)] = encodeAccessFile(entry);
    addSecret(created.files, created.id, 0, author.userKey, created.capability, author);
    Board document;
    document.setName(create.name.toStdString());
    created.files[changeFilePath(author.certificate.deviceKey)] =
        sealedContent(document, created.id, ContentKind::Changes,
                      toByteArray(document.encodeState()), author, {}, {document.clientId()});
    return created;
}

QByteArray changeFile(const Board &document, const LoadedBoard &loaded, const Author &author,
                      const std::vector<std::uint64_t> &clients) {
    const QByteArray known = encodeStateVector(loaded.snapshotState());
    const QByteArray body = toByteArray(document.difference(byteView(known)));
    return sealedContent(document, loaded.info.board, ContentKind::Changes, body, author,
                         snapshotHashes(loaded), clients);
}

bool changesCovered(const LoadedBoard &loaded, const QByteArray &device) {
    const auto own = std::ranges::find(loaded.changes, device, &AcceptedContent::device);
    if (own == loaded.changes.end())
        return true;
    if (!covers(loaded.snapshotState(), own->state))
        return false;
    // Deletions move no state vector, so the file is covered only if applying it to the
    // snapshots changes nothing.
    Board snapshots;
    for (const AcceptedContent &snapshot : loaded.snapshots)
        snapshots.apply(byteView(snapshot.body));
    const Bytes before = snapshots.encodeState();
    snapshots.apply(byteView(own->body));
    return snapshots.encodeState() == before;
}

Compaction compact(const Board &document, const LoadedBoard &loaded, const Author &author,
                   const std::vector<std::uint64_t> &clients) {
    const QByteArray snapshot =
        sealedContent(document, loaded.info.board, ContentKind::Snapshot,
                      toByteArray(document.encodeState()), author, snapshotHashes(loaded), clients);
    Compaction result{sha256(snapshot), {}};
    result.files[snapshotPath(result.hash)] = snapshot;
    for (const AcceptedContent &covered : loaded.snapshots)
        result.files[covered.path] = std::nullopt;
    result.files[changeFilePath(author.certificate.deviceKey)] = std::nullopt;
    return result;
}

FileChanges setRole(const LoadedBoard &loaded, const Author &author, const QByteArray &capability,
                    const QByteArray &user, const QString &name, Role role) {
    if (role == Role::None)
        throw std::invalid_argument("Use removeMember to take someone off a board");
    AccessOp op;
    op.action = AccessOp::Action::SetRole;
    op.user = user;
    op.name = name;
    op.role = role;
    const SignedOp entry = signedEntry(loaded, author, op);
    FileChanges files{{QStringLiteral("access/") + accessFileName(entry), encodeAccessFile(entry)}};
    addSecret(files, loaded.info.board, std::max(loaded.latestEpoch(), 0), user, capability,
              author);
    return files;
}

FileChanges removeMember(const Board &document, const LoadedBoard &loaded, const Author &author,
                         const QByteArray &user, const std::vector<std::uint64_t> &clients,
                         const QByteArray &newCapability) {
    Compaction snapshot = compact(document, loaded, author, clients);
    AccessOp op;
    op.action = AccessOp::Action::SetRole;
    op.user = user;
    op.role = Role::None;
    op.snapshot = snapshot.hash;
    const SignedOp entry = signedEntry(loaded, author, op);
    FileChanges files = std::move(snapshot.files);
    files[QStringLiteral("access/") + accessFileName(entry)] = encodeAccessFile(entry);
    const int epoch = loaded.latestEpoch() + 1;
    for (const auto &[member, role] : loaded.access.roles)
        if (member != user && role >= Role::Member)
            addSecret(files, loaded.info.board, epoch, member, newCapability, author);
    return files;
}

QByteArray freshCapability() { return newCapability(); }

std::optional<QByteArray> openSecret(const LoadedBoard &loaded, const UserIdentity &identity) {
    for (auto epoch = loaded.secrets.rbegin(); epoch != loaded.secrets.rend(); ++epoch) {
        const BoardSecret *chosen = nullptr;
        for (const BoardSecret &secret : epoch->second)
            if (secret.user == identity.userKey() && (!chosen || secret.signer < chosen->signer))
                chosen = &secret;
        if (chosen)
            return identity.openSealed(chosen->sealed, maxCapabilityBytes);
    }
    return std::nullopt;
}

} // namespace peerchalk::repository
