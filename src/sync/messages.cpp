#include "sync/messages.hpp"

#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>

#include <QtEndian>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace peerchalk::sync {
namespace {

enum Type : char {
    HelloType = 1,
    AccessType = 2,
    PersonalType = 3,
    BoardType = 4,
    PresenceType = 5,
    ProfileType = 6,
    LiveStrokeType = 7,
    PartType = 8,
    BoardStateType = 9,
    BoardLinkJoinType = 10,
    InstanceHelloType = 11
};
constexpr qsizetype uuidSize = 16;
constexpr qsizetype partHeaderSize = 1 + 4 + 4;

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

// Live strokes are little-endian with 32-bit floats.
void prepare(QDataStream &stream) {
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
}

struct Encoder {
    QByteArray operator()(const Hello &hello) const {
        return json(HelloType, {{"certificate", base64(hello.certificatePayload)},
                                {"signature", base64(hello.certificateSignature)},
                                {"name", hello.displayName},
                                {"color", hello.colorIndex},
                                {"protocol", hello.protocol},
                                {"minProtocol", hello.minProtocol}});
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
    QByteArray operator()(const BoardState &state) const {
        return QByteArray(1, BoardStateType) + state.board.toRfc4122() + state.stateVector;
    }
    QByteArray operator()(const BoardLinkJoin &join) const {
        return QByteArray(1, BoardLinkJoinType) + join.board.toRfc4122() + join.token;
    }
    QByteArray operator()(const InstanceHello &) const { return QByteArray(1, InstanceHelloType); }
    QByteArray operator()(const MessagePart &part) const {
        QByteArray bytes(partHeaderSize, '\0');
        bytes[0] = PartType;
        qToLittleEndian(part.index, bytes.data() + 1);
        qToLittleEndian(part.count, bytes.data() + 5);
        return bytes + part.data;
    }
    QByteArray operator()(const UnknownMessage &) const {
        throw std::logic_error("Unknown messages are never sent");
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
                                   {"cursor", presence.cursorVisible},
                                   {"seq", static_cast<qint64>(presence.sequence)}});
    }
    QByteArray operator()(const LiveStroke &stroke) const {
        QByteArray bytes(1, LiveStrokeType);
        bytes += stroke.board.toRfc4122();
        QDataStream out(&bytes, QIODevice::WriteOnly | QIODevice::Append);
        prepare(out);
        out << stroke.stroke << stroke.firstPoint << static_cast<quint8>(stroke.state)
            << static_cast<quint8>(stroke.kind) << stroke.argb << stroke.width
            << static_cast<quint16>(stroke.points.size());
        for (const StrokePoint &point : stroke.points)
            out << point.x << point.y << point.pressure;
        return bytes;
    }
};

std::optional<Message> decodeHello(const QJsonObject &object) {
    const auto payload = fromBase64(object[QStringLiteral("certificate")]);
    const auto signature = fromBase64(object[QStringLiteral("signature")]);
    if (!payload || !signature)
        return std::nullopt;
    return Hello{*payload,
                 *signature,
                 object[QStringLiteral("name")].toString().left(200),
                 object[QStringLiteral("color")].toInt(),
                 object[QStringLiteral("protocol")].toInt(1),
                 object[QStringLiteral("minProtocol")].toInt(1)};
}

std::optional<Message> decodePart(const QByteArray &bytes) {
    if (bytes.size() <= partHeaderSize)
        return std::nullopt;
    MessagePart part{qFromLittleEndian<quint32>(bytes.constData() + 1),
                     qFromLittleEndian<quint32>(bytes.constData() + 5), bytes.mid(partHeaderSize)};
    if (part.index >= part.count)
        return std::nullopt;
    return part;
}

std::optional<Message> decodeBoardState(const QByteArray &bytes) {
    if (bytes.size() <= 1 + uuidSize)
        return std::nullopt;
    const QUuid board = QUuid::fromRfc4122(bytes.mid(1, uuidSize));
    if (board.isNull())
        return std::nullopt;
    return BoardState{board, bytes.mid(1 + uuidSize)};
}

std::optional<Message> decodeBoardLinkJoin(const QByteArray &bytes) {
    if (bytes.size() != 1 + uuidSize + 32)
        return std::nullopt;
    const QUuid board = QUuid::fromRfc4122(bytes.mid(1, uuidSize));
    if (board.isNull())
        return std::nullopt;
    return BoardLinkJoin{board, bytes.mid(1 + uuidSize)};
}

bool isJsonType(char type) {
    return type == HelloType || type == AccessType || type == PersonalType ||
           type == PresenceType || type == ProfileType;
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
    const qint64 sequence = object[QStringLiteral("seq")].toInteger();
    if (sequence < 0 || sequence > std::numeric_limits<quint32>::max())
        return std::nullopt;
    return Presence{
        id, {x, y}, object[QStringLiteral("cursor")].toBool(), static_cast<quint32>(sequence)};
}

bool validPoint(const StrokePoint &point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && point.pressure >= 0.0F &&
           point.pressure <= 1.0F;
}

std::optional<Message> decodeLiveStroke(const QByteArray &bytes) {
    if (bytes.size() <= 1 + uuidSize)
        return std::nullopt;
    LiveStroke stroke;
    stroke.board = QUuid::fromRfc4122(bytes.mid(1, uuidSize));
    QDataStream in(bytes.mid(1 + uuidSize));
    prepare(in);
    quint8 state = 0;
    quint8 kind = 0;
    quint16 count = 0;
    in >> stroke.stroke >> stroke.firstPoint >> state >> kind >> stroke.argb >> stroke.width >>
        count;
    if (in.status() != QDataStream::Ok || stroke.board.isNull() || state > 2 || kind > 1 ||
        count > maxLiveStrokePoints || !std::isfinite(stroke.width) || stroke.width <= 0.0F)
        return std::nullopt;
    stroke.state = static_cast<DraftState>(state);
    stroke.kind = static_cast<StrokeKind>(kind);
    stroke.points.resize(count);
    for (StrokePoint &point : stroke.points) {
        in >> point.x >> point.y >> point.pressure;
        if (in.status() != QDataStream::Ok || !validPoint(point))
            return std::nullopt;
    }
    if (!in.atEnd() || stroke.firstPoint > std::numeric_limits<quint32>::max() - count)
        return std::nullopt;
    return stroke;
}

} // namespace

QByteArray encode(const Message &message) { return std::visit(Encoder{}, message); }

bool protocolsCompatible(const Hello &hello) {
    return hello.protocol >= minSupportedProtocol && hello.minProtocol <= protocolVersion;
}

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
    if (type == LiveStrokeType)
        return decodeLiveStroke(bytes);
    if (type == PartType)
        return decodePart(bytes);
    if (type == BoardStateType)
        return decodeBoardState(bytes);
    if (type == BoardLinkJoinType)
        return decodeBoardLinkJoin(bytes);
    if (type == InstanceHelloType)
        return InstanceHello{};
    if (!isJsonType(type))
        return UnknownMessage{};
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
