#include "hosted/hosted_messages.hpp"

#include <QJsonArray>

namespace peerchalk::hosted {
namespace {

constexpr qsizetype keySize = 32;

repository::BoardFiles filesOf(const QJsonObject &object) {
    repository::BoardFiles files;
    for (auto it = object.begin(); it != object.end(); ++it)
        files[it.key()] = bytesOf(it.value(), "a board file");
    return files;
}

std::vector<QByteArray> accessOf(const QJsonValue &value) {
    std::vector<QByteArray> access;
    for (const QJsonValue &entry : value.toArray())
        access.push_back(bytesOf(entry, "an access entry"));
    return access;
}

QByteArray exactBytes(const QJsonValue &value, const char *what) {
    QByteArray bytes = bytesOf(value, what);
    if (bytes.size() != keySize)
        throw MessageError(std::string(what) + " must be 32 bytes");
    return bytes;
}

std::vector<std::uint64_t> clientsOf(const QJsonValue &value) {
    std::vector<std::uint64_t> clients;
    for (const QJsonValue &client : value.toArray()) {
        bool ok = false;
        clients.push_back(client.toString().toULongLong(&ok));
        if (!ok)
            throw MessageError("a client id is malformed");
    }
    return clients;
}

} // namespace

QByteArray bytesOf(const QJsonValue &value, const char *what) {
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        value.toObject()[QStringLiteral("$bytes")].toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (!value.isObject() || !decoded)
        throw MessageError(std::string(what) + " is not bytes");
    return decoded.decoded;
}

QJsonValue bytesValue(const QByteArray &bytes) {
    return QJsonObject{{"$bytes", QString::fromLatin1(bytes.toBase64())}};
}

OpenMessage parseOpen(const QJsonObject &message) {
    if (message["protocol"].toInt() != 1)
        throw MessageError("this PeerChalk speaks version 1 of the hosted messages");
    const QJsonObject board = message["board"].toObject();
    const QJsonObject identity = message["identity"].toObject();
    OpenMessage open{QUuid::fromString(board["id"].toString()),
                     filesOf(board["files"].toObject()),
                     accessOf(message["access"]),
                     exactBytes(message["secret"], "the board's secret"),
                     identity["transferCode"].toString(),
                     exactBytes(identity["deviceSeed"], "the device seed"),
                     exactBytes(identity["recipientSecret"], "the recipient secret"),
                     identity["deviceName"].toString(),
                     identity["name"].toString().trimmed(),
                     identity["colorIndex"].toInt(),
                     message["canPush"].toBool(),
                     clientsOf(message["clients"])};
    if (open.board.isNull() || open.transferCode.isEmpty())
        throw MessageError("open needs a board id and an identity");
    return open;
}

UpdateMessage parseUpdate(const QJsonObject &message) {
    UpdateMessage update;
    const QJsonObject files = message["files"].toObject();
    for (auto it = files.begin(); it != files.end(); ++it)
        update.files[it.key()] =
            it.value().isNull() ? std::nullopt : std::optional(bytesOf(it.value(), "a board file"));
    update.access = accessOf(message["access"]);
    if (message.contains("canPush"))
        update.canPush = message["canPush"].toBool();
    return update;
}

} // namespace peerchalk::hosted
