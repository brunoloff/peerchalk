#include "library/signatures.hpp"

#include "library/codes.hpp"

#include <p2pdc/security.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>
#include <variant>

namespace peerchalk {
namespace {

// Signed payloads start with a type tag, so a signature for one kind of statement can
// never be replayed as another.
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

} // namespace

bool verifySignature(const QByteArray &signingKey, const QByteArray &message,
                     const QByteArray &signature) {
    if (signingKey.size() != signingKeySize || signature.size() != signatureSize)
        return false;
    return std::holds_alternative<std::monostate>(
        p2pdc::verify_signature(view(signingKey), view(message), view(signature)));
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
