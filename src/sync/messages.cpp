#include "sync/messages.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>

namespace peerchalk::sync {
namespace {

enum Type : char {
    HelloType = 1,
    AccessType = 2,
    PersonalType = 3,
    BoardType = 4,
    PresenceType = 5,
    ProfileType = 6
};
constexpr qsizetype uuidSize = 16;

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

std::optional<QByteArray> fromBase64(const QJsonValue &value) {
    if (!value.isString())
        return std::nullopt;
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        return std::nullopt;
    return decoded.decoded;
}

QByteArray json(Type type, const QJsonObject &object) {
    return QByteArray(1, type) + QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<QJsonObject> jsonBody(const QByteArray &bytes) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes.mid(1), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

struct Encoder {
    QByteArray operator()(const Hello &hello) const {
        return json(HelloType, {{"certificate", base64(hello.certificatePayload)},
                                {"signature", base64(hello.certificateSignature)},
                                {"name", hello.displayName},
                                {"color", hello.colorIndex}});
    }
    QByteArray operator()(const AccessChanges &message) const {
        QJsonArray ops;
        for (const auto &[payload, signature] : message.ops)
            ops.append(QJsonObject{{"payload", base64(payload)}, {"signature", base64(signature)}});
        return json(AccessType, {{"ops", ops}});
    }
    QByteArray operator()(const Personal &personal) const {
        return json(PersonalType,
                    {{"workspace", personal.workspace}, {"contacts", personal.contacts}});
    }
    QByteArray operator()(const BoardUpdate &update) const {
        return QByteArray(1, BoardType) + update.board.toRfc4122() + update.update;
    }
    QByteArray operator()(const Profile &profile) const {
        return json(ProfileType, {{"name", profile.displayName}, {"color", profile.colorIndex}});
    }
    QByteArray operator()(const Presence &presence) const {
        return json(PresenceType, {{"board", presence.board.isNull()
                                                 ? QString()
                                                 : presence.board.toString(QUuid::WithoutBraces)},
                                   {"x", presence.cursor.x()},
                                   {"y", presence.cursor.y()},
                                   {"cursor", presence.cursorVisible}});
    }
};

std::optional<Message> decodeHello(const QJsonObject &object) {
    const auto payload = fromBase64(object[QStringLiteral("certificate")]);
    const auto signature = fromBase64(object[QStringLiteral("signature")]);
    if (!payload || !signature)
        return std::nullopt;
    return Hello{*payload, *signature, object[QStringLiteral("name")].toString().left(200),
                 object[QStringLiteral("color")].toInt()};
}

std::optional<Message> decodeAccess(const QJsonObject &object) {
    AccessChanges message;
    for (const QJsonValue &value : object[QStringLiteral("ops")].toArray()) {
        const auto payload = fromBase64(value[QStringLiteral("payload")]);
        const auto signature = fromBase64(value[QStringLiteral("signature")]);
        if (!payload || !signature)
            return std::nullopt;
        message.ops.emplace_back(*payload, *signature);
    }
    return message;
}

std::optional<Message> decodePresence(const QJsonObject &object) {
    const QString board = object[QStringLiteral("board")].toString();
    const double x = object[QStringLiteral("x")].toDouble();
    const double y = object[QStringLiteral("y")].toDouble();
    if (!std::isfinite(x) || !std::isfinite(y))
        return std::nullopt;
    const QUuid id = board.isEmpty() ? QUuid() : QUuid::fromString(board);
    if (!board.isEmpty() && id.isNull())
        return std::nullopt;
    return Presence{id, {x, y}, object[QStringLiteral("cursor")].toBool()};
}

} // namespace

QByteArray encode(const Message &message) { return std::visit(Encoder{}, message); }

std::optional<Message> decode(const QByteArray &bytes) {
    if (bytes.isEmpty())
        return std::nullopt;
    const char type = bytes.at(0);
    if (type == BoardType) {
        if (bytes.size() <= 1 + uuidSize)
            return std::nullopt;
        const QUuid board = QUuid::fromRfc4122(bytes.mid(1, uuidSize));
        if (board.isNull())
            return std::nullopt;
        return BoardUpdate{board, bytes.mid(1 + uuidSize)};
    }
    const std::optional<QJsonObject> object = jsonBody(bytes);
    if (!object)
        return std::nullopt;
    switch (type) {
    case HelloType:
        return decodeHello(*object);
    case AccessType:
        return decodeAccess(*object);
    case PersonalType:
        return Personal{(*object)[QStringLiteral("workspace")].toObject(),
                        (*object)[QStringLiteral("contacts")].toObject()};
    case PresenceType:
        return decodePresence(*object);
    case ProfileType:
        return Profile{(*object)[QStringLiteral("name")].toString().left(200),
                       (*object)[QStringLiteral("color")].toInt()};
    default:
        return std::nullopt;
    }
}

} // namespace peerchalk::sync
