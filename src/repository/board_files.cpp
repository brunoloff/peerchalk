#include "repository/board_files.hpp"

#include "library/codes.hpp"
#include "library/secrets.hpp"
#include "repository/encoding.hpp"

#include <p2pdc/recipient_crypto.h>

#include <QJsonArray>
#include <QJsonDocument>

namespace peerchalk::repository {
namespace {

constexpr qsizetype maxSecretBytes = 1024;

QByteArray compact(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// {"payload": <compact JSON as a string>, "signature": <base64>}, readable in diffs.
QByteArray signedJson(const QJsonObject &payload, const Signer &signer) {
    const QByteArray bytes = compact(payload);
    return compact({{"payload", QString::fromUtf8(bytes)}, {"signature", base64(signer(bytes))}}) +
           '\n';
}

struct SignedJson {
    QByteArray payload;
    QByteArray signature;
    QJsonObject object;
};

SignedJson openSignedJson(const QByteArray &file, const char *what) {
    const QJsonObject outer = parseObject(file, what);
    SignedJson result{outer["payload"].toString().toUtf8(), base64Field(outer, "signature"), {}};
    result.object = parseObject(result.payload, what);
    return result;
}

void requireFormat(const QJsonObject &object, const char *format) {
    if (object["format"].toString() != QLatin1String(format))
        throw FileError(std::string("this is not a ") + format + " file");
    if (object["version"].toInt() != boardFormatVersion)
        throw FileError(std::string("this ") + format + " file is from a newer version");
}

} // namespace

QByteArray encodeBoardInfo(const BoardInfo &info) {
    return compact({{"format", "peerchalk-repository-board"},
                    {"version", boardFormatVersion},
                    {"board", info.board.toString(QUuid::WithoutBraces)},
                    {"creator", toHex(info.creator)}}) +
           '\n';
}

BoardInfo decodeBoardInfo(const QByteArray &file) {
    const QJsonObject object = parseObject(file, "board.json");
    requireFormat(object, "peerchalk-repository-board");
    BoardInfo info{QUuid::fromString(object["board"].toString()),
                   hexField(object, "creator", signingKeySize)};
    if (info.board.isNull())
        throw FileError("board.json names no board");
    return info;
}

QByteArray encodeAccessFile(const SignedOp &op) {
    return compact(
               {{"payload", QString::fromUtf8(op.payload)}, {"signature", base64(op.signature)}}) +
           '\n';
}

SignedOp decodeAccessFile(const QByteArray &file) {
    const QJsonObject outer = parseObject(file, "the access entry");
    const std::optional<SignedOp> op =
        verifyOp(outer["payload"].toString().toUtf8(), base64Field(outer, "signature"));
    if (!op)
        throw FileError("the access entry is malformed or its signature does not match");
    return *op;
}

QString accessFileName(const SignedOp &op) { return toHex(op.hash) + QStringLiteral(".op"); }

QByteArray encodeSecretFile(const BoardSecret &secret, const Signer &signer) {
    return signedJson({{"format", "peerchalk-board-secret"},
                       {"version", boardFormatVersion},
                       {"board", secret.board.toString(QUuid::WithoutBraces)},
                       {"epoch", secret.epoch},
                       {"user", toHex(secret.user)},
                       {"sealed", base64(secret.sealed)},
                       {"signer", toHex(secret.signer)}},
                      signer);
}

BoardSecret decodeSecretFile(const QByteArray &file) {
    const SignedJson parts = openSignedJson(file, "the secret");
    requireFormat(parts.object, "peerchalk-board-secret");
    BoardSecret secret{
        QUuid::fromString(parts.object["board"].toString()), parts.object["epoch"].toInt(-1),
        hexField(parts.object, "user", signingKeySize), base64Field(parts.object, "sealed"),
        hexField(parts.object, "signer", signingKeySize)};
    if (secret.board.isNull() || secret.epoch < 0 || secret.sealed.size() > maxSecretBytes)
        throw FileError("the secret is malformed");
    if (!verifySignature(secret.signer, parts.payload, parts.signature))
        throw FileError("the secret's signature does not match");
    return secret;
}

QString secretFileName(const BoardSecret &secret) {
    return QString::number(secret.epoch) + QLatin1Char('/') + toHex(secret.signer) +
           QLatin1Char('/') + toHex(secret.user);
}

QByteArray sealCapability(const QByteArray &userKey, const QByteArray &capability) {
    return peerchalk::toBytes(peerchalk::unwrap(
        p2pdc::seal_to_signing_key(peerchalk::byteView(userKey), peerchalk::byteView(capability),
                                   static_cast<std::size_t>(capability.size())),
        "Could not seal the board's secret to that user key"));
}

QByteArray encodePerson(const Person &person, const Signer &userSigner) {
    QJsonArray devices;
    for (const DeviceCertificate &device : person.devices)
        devices.append(QJsonObject{{"device", toHex(device.deviceKey)},
                                   {"payload", base64(device.payload)},
                                   {"signature", base64(device.signature)}});
    QJsonObject payload{{"format", "peerchalk-person"},
                        {"version", boardFormatVersion},
                        {"userKey", toHex(person.userKey)},
                        {"name", person.name},
                        {"devices", devices}};
    if (!person.email.isEmpty())
        payload["email"] = person.email;
    return signedJson(payload, userSigner);
}

Person decodePerson(const QByteArray &file) {
    const SignedJson parts = openSignedJson(file, "the person file");
    requireFormat(parts.object, "peerchalk-person");
    Person person{hexField(parts.object, "userKey", signingKeySize),
                  parts.object["name"].toString().trimmed().left(200),
                  parts.object["email"].toString().trimmed().left(200),
                  {}};
    if (!verifySignature(person.userKey, parts.payload, parts.signature))
        throw FileError("the person file's signature does not match");
    for (const QJsonValue &value : parts.object["devices"].toArray()) {
        const QJsonObject device = value.toObject();
        const std::optional<DeviceCertificate> checked = verifyDeviceCertificate(
            base64Field(device, "payload"), base64Field(device, "signature"),
            hexField(device, "device", signingKeySize));
        if (!checked || checked->userKey != person.userKey)
            throw FileError("a device certificate in the person file is invalid");
        person.devices.push_back(*checked);
    }
    return person;
}

} // namespace peerchalk::repository
