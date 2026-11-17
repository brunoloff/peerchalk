#include "library/manifest.hpp"

#include "library/codes.hpp"

#include <p2pdc/security.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace peerchalk {
namespace {

// Signed payloads start with a type tag, so a signature for one kind of statement can
// never be replayed as another.
const QString manifestType = QStringLiteral("peerchalk/manifest/v1");
const QString certificateType = QStringLiteral("peerchalk/device-certificate/v1");
constexpr qsizetype signatureSize = 64;
constexpr qsizetype maxPayloadSize = 256 * 1024;

p2pdc::ByteView view(const QByteArray &bytes) {
    return {reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

std::optional<QByteArray> key(const QJsonValue &value) {
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() != signingKeySize)
        return std::nullopt;
    return decoded.decoded;
}

QJsonObject toJson(const Manifest &manifest) {
    QJsonArray members;
    for (const Member &member : manifest.members)
        members.append(QJsonObject{{"key", base64(member.key)}, {"name", member.name}});
    QJsonArray projects;
    for (const QUuid &project : manifest.projects)
        projects.append(project.toString(QUuid::WithoutBraces));
    return {{"type", manifestType},
            {"kind", manifest.kind == ResourceKind::Project ? "project" : "board"},
            {"id", manifest.id.toString(QUuid::WithoutBraces)},
            {"name", manifest.name},
            {"admin", base64(manifest.admin)},
            {"version", QString::number(manifest.version)},
            {"members", members},
            {"projects", projects},
            {"deleted", manifest.deleted}};
}

std::optional<std::vector<Member>> parseMembers(const QJsonArray &array) {
    std::vector<Member> members;
    for (const QJsonValue &value : array) {
        const std::optional<QByteArray> memberKey = key(value[QStringLiteral("key")]);
        const QString name = value[QStringLiteral("name")].toString().trimmed();
        if (!memberKey || name.isEmpty())
            return std::nullopt;
        members.push_back({*memberKey, name});
    }
    return members;
}

std::optional<Manifest> parseManifest(const QByteArray &payload) {
    if (payload.size() > maxPayloadSize)
        return std::nullopt;
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QString kind = object[QStringLiteral("kind")].toString();
    bool versionOk = false;
    Manifest manifest;
    manifest.kind = kind == QStringLiteral("board") ? ResourceKind::Board : ResourceKind::Project;
    manifest.id = QUuid::fromString(object[QStringLiteral("id")].toString());
    manifest.name = object[QStringLiteral("name")].toString().trimmed();
    manifest.version = object[QStringLiteral("version")].toString().toULongLong(&versionOk);
    manifest.deleted = object[QStringLiteral("deleted")].toBool();
    const std::optional<QByteArray> admin = key(object[QStringLiteral("admin")]);
    const auto members = parseMembers(object[QStringLiteral("members")].toArray());
    if (object[QStringLiteral("type")].toString() != manifestType ||
        (kind != QStringLiteral("project") && kind != QStringLiteral("board")) ||
        manifest.id.isNull() || manifest.name.isEmpty() || !versionOk || !admin || !members)
        return std::nullopt;
    manifest.admin = *admin;
    manifest.members = *members;
    for (const QJsonValue &project : object[QStringLiteral("projects")].toArray()) {
        const QUuid projectId = QUuid::fromString(project.toString());
        if (projectId.isNull())
            return std::nullopt;
        manifest.projects.push_back(projectId);
    }
    return manifest;
}

} // namespace

bool verifySignature(const QByteArray &signingKey, const QByteArray &message,
                     const QByteArray &signature) {
    if (signingKey.size() != signingKeySize || signature.size() != signatureSize)
        return false;
    return std::holds_alternative<std::monostate>(
        p2pdc::verify_signature(view(signingKey), view(message), view(signature)));
}

bool Manifest::includes(const QByteArray &userKey) const {
    return userKey == admin ||
           std::ranges::any_of(members, [&](const Member &m) { return m.key == userKey; });
}

SignedManifest signManifest(const Manifest &manifest, const Signer &signer) {
    if (manifest.id.isNull() || manifest.name.trimmed().isEmpty() ||
        manifest.admin.size() != signingKeySize)
        throw std::invalid_argument("A manifest needs an id, a name and an admin key");
    const QByteArray payload = QJsonDocument(toJson(manifest)).toJson(QJsonDocument::Compact);
    return {manifest, payload, signer(payload)};
}

std::optional<SignedManifest> verifyAnyManifest(const QByteArray &payload,
                                                const QByteArray &signature) {
    const std::optional<Manifest> manifest = parseManifest(payload);
    if (!manifest || !verifySignature(manifest->admin, payload, signature))
        return std::nullopt;
    return SignedManifest{*manifest, payload, signature};
}

std::optional<SignedManifest> verifyManifest(const QByteArray &payload, const QByteArray &signature,
                                             ResourceKind kind, const QUuid &id,
                                             const QByteArray &admin) {
    std::optional<SignedManifest> signedManifest = verifyAnyManifest(payload, signature);
    if (!signedManifest || signedManifest->manifest.kind != kind ||
        signedManifest->manifest.id != id || signedManifest->manifest.admin != admin)
        return std::nullopt;
    return signedManifest;
}

DeviceCertificate certifyDevice(const QByteArray &userKey, const QByteArray &deviceKey,
                                const QString &deviceName, const Signer &signer) {
    if (userKey.size() != signingKeySize || deviceKey.size() != signingKeySize)
        throw std::invalid_argument("A device certificate needs two 32-byte keys");
    const QByteArray payload = QJsonDocument(QJsonObject{{"type", certificateType},
                                                         {"user", base64(userKey)},
                                                         {"device", base64(deviceKey)},
                                                         {"name", deviceName}})
                                   .toJson(QJsonDocument::Compact);
    return {userKey, deviceKey, deviceName, payload, signer(payload)};
}

std::optional<DeviceCertificate> verifyDeviceCertificate(const QByteArray &payload,
                                                         const QByteArray &signature,
                                                         const QByteArray &deviceKey) {
    if (payload.size() > maxPayloadSize)
        return std::nullopt;
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const std::optional<QByteArray> user = key(object[QStringLiteral("user")]);
    const std::optional<QByteArray> device = key(object[QStringLiteral("device")]);
    if (object[QStringLiteral("type")].toString() != certificateType || !user || !device ||
        *device != deviceKey || !verifySignature(*user, payload, signature))
        return std::nullopt;
    return DeviceCertificate{*user, *device, object[QStringLiteral("name")].toString(), payload,
                             signature};
}

} // namespace peerchalk
