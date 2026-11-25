#include "repository/encoding.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>

namespace peerchalk::repository {

QString toHex(const QByteArray &bytes) { return QString::fromLatin1(bytes.toHex()); }

std::optional<QByteArray> fromHex(const QString &text, qsizetype size) {
    static const QRegularExpression lowerHex(QStringLiteral("^[0-9a-f]*$"));
    if (text.size() != size * 2 || !lowerHex.match(text).hasMatch())
        return std::nullopt;
    return QByteArray::fromHex(text.toLatin1());
}

QByteArray sha256(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QByteArray toByteArray(ByteView bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

ByteView byteView(const QByteArray &bytes) {
    return {reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

QJsonObject parseObject(const QByteArray &bytes, const char *what) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw FileError(std::string(what) + " is not a JSON object");
    return document.object();
}

QByteArray hexField(const QJsonObject &object, const char *key, qsizetype size) {
    const std::optional<QByteArray> value = fromHex(object[QLatin1String(key)].toString(), size);
    if (!value)
        throw FileError(std::string("the field \"") + key + "\" is missing or malformed");
    return *value;
}

QByteArray base64Field(const QJsonObject &object, const char *key) {
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        object[QLatin1String(key)].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        throw FileError(std::string("the field \"") + key + "\" is not base64");
    return decoded.decoded;
}

QString base64(const QByteArray &bytes) { return QString::fromLatin1(bytes.toBase64()); }

} // namespace peerchalk::repository
