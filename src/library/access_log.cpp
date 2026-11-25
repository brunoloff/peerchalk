#include "library/access_log.hpp"

#include "library/codes.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace peerchalk {
namespace {

// Signed payloads start with a type tag, so a signature for one kind of statement can
// never be replayed as another.
const QString opType = QStringLiteral("peerchalk/access-op/v1");
constexpr qsizetype maxPayloadSize = 16 * 1024;
const std::array<QString, 7> actionNames = {QStringLiteral("create"), QStringLiteral("rename"),
                                            QStringLiteral("role"),   QStringLiteral("admin"),
                                            QStringLiteral("board"),  QStringLiteral("delete"),
                                            QStringLiteral("link")};
const std::array<QString, 3> roleNames = {QStringLiteral("none"), QStringLiteral("member"),
                                          QStringLiteral("owner")};

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

std::optional<QByteArray> key(const QJsonValue &value) {
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() != signingKeySize)
        return std::nullopt;
    return decoded.decoded;
}

template <std::size_t N> int indexOf(const std::array<QString, N> &names, const QString &name) {
    const auto it = std::ranges::find(names, name);
    return it == names.end() ? -1 : static_cast<int>(it - names.begin());
}

QJsonObject toJson(const AccessOp &op) {
    QJsonObject object{{"type", opType},
                       {"kind", op.kind == ResourceKind::Project ? "project" : "board"},
                       {"resource", op.resource.toString(QUuid::WithoutBraces)},
                       {"clock", QString::number(op.clock)},
                       {"signer", base64(op.signer)},
                       {"action", actionNames.at(static_cast<std::size_t>(op.action))}};
    if (!op.name.isEmpty())
        object["name"] = op.name;
    if (!op.signerName.isEmpty())
        object["signerName"] = op.signerName;
    if (!op.user.isEmpty())
        object["user"] = base64(op.user);
    if (op.action == AccessOp::Action::SetRole)
        object["role"] = roleNames.at(static_cast<std::size_t>(op.role));
    if (op.action == AccessOp::Action::SetRole && !op.snapshot.isEmpty())
        object["snapshot"] = base64(op.snapshot);
    if (op.action == AccessOp::Action::SetLink && !op.linkHash.isEmpty())
        object["linkHash"] = base64(op.linkHash);
    if (op.action == AccessOp::Action::SetBoard) {
        object["board"] = op.board.toString(QUuid::WithoutBraces);
        object["included"] = op.included;
    }
    return object;
}

// A SetRole change's role and, for removals from repository boards, the snapshot.
bool parseRole(const QJsonObject &object, AccessOp &op) {
    const int role = indexOf(roleNames, object["role"].toString());
    if (role < 0)
        return false;
    op.role = static_cast<Role>(role);
    if (!object.contains("snapshot"))
        return true;
    const std::optional<QByteArray> snapshot = key(object["snapshot"]);
    if (!snapshot)
        return false;
    op.snapshot = *snapshot;
    return true;
}

// The action-specific fields of a parsed change; false when they are missing.
bool parseFields(const QJsonObject &object, AccessOp &op) {
    using Action = AccessOp::Action;
    op.name = object["name"].toString().trimmed().left(200);
    op.signerName = object["signerName"].toString().trimmed().left(200);
    if (op.action == Action::SetRole || op.action == Action::TransferAdmin ||
        op.action == Action::SetBoard) {
        const std::optional<QByteArray> user = key(object["user"]);
        if (!user)
            return false;
        op.user = *user;
    }
    if (op.action == Action::SetRole && !parseRole(object, op))
        return false;
    if (op.action == Action::SetBoard) {
        op.board = QUuid::fromString(object["board"].toString());
        op.included = object["included"].toBool();
        if (op.board.isNull() || op.kind != ResourceKind::Project)
            return false;
    }
    if (op.action == Action::SetLink) {
        if (op.kind != ResourceKind::Board)
            return false;
        if (object.contains("linkHash")) {
            const std::optional<QByteArray> hash = key(object["linkHash"]);
            if (!hash)
                return false;
            op.linkHash = *hash;
        }
    }
    return (op.action != Action::Create && op.action != Action::Rename) || !op.name.isEmpty();
}

std::optional<AccessOp> parse(const QByteArray &payload) {
    if (payload.size() > maxPayloadSize)
        return std::nullopt;
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QString kind = object["kind"].toString();
    const int action = indexOf(actionNames, object["action"].toString());
    const std::optional<QByteArray> signer = key(object["signer"]);
    bool clockOk = false;
    AccessOp op;
    op.kind = kind == QStringLiteral("board") ? ResourceKind::Board : ResourceKind::Project;
    op.resource = QUuid::fromString(object["resource"].toString());
    op.clock = object["clock"].toString().toULongLong(&clockOk);
    if (object["type"].toString() != opType ||
        (kind != QStringLiteral("project") && kind != QStringLiteral("board")) || action < 0 ||
        !signer || op.resource.isNull() || !clockOk)
        return std::nullopt;
    op.signer = *signer;
    op.action = static_cast<AccessOp::Action>(action);
    if (!parseFields(object, op))
        return std::nullopt;
    return op;
}

} // namespace

SignedOp signOp(const AccessOp &op, const Signer &signer) {
    if (op.resource.isNull() || op.signer.size() != signingKeySize)
        throw std::invalid_argument("A change needs a resource and a signer");
    const QByteArray payload = QJsonDocument(toJson(op)).toJson(QJsonDocument::Compact);
    return {op, payload, signer(payload),
            QCryptographicHash::hash(payload, QCryptographicHash::Sha256)};
}

std::optional<SignedOp> verifyOp(const QByteArray &payload, const QByteArray &signature) {
    const std::optional<AccessOp> op = parse(payload);
    if (!op || !verifySignature(op->signer, payload, signature))
        return std::nullopt;
    return SignedOp{*op, payload, signature,
                    QCryptographicHash::hash(payload, QCryptographicHash::Sha256)};
}

Role AccessState::role(const QByteArray &user) const {
    const auto it = roles.find(user);
    return it == roles.end() ? Role::None : it->second;
}

} // namespace peerchalk
