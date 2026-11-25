#include "repository/signed_content.hpp"

#include "library/codes.hpp"
#include "repository/encoding.hpp"

#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>

namespace peerchalk::repository {
namespace {

const QByteArray magic = QByteArrayLiteral("PCRB\x01");
constexpr qsizetype signatureSize = 64;
constexpr quint32 maxHeaderBytes = 64 * 1024;
constexpr quint32 maxBodyBytes = 256U * 1024 * 1024;
constexpr qsizetype hashSize = 32;

QByteArray headerJson(const ContentHeader &header) {
    QJsonArray covers;
    for (const QByteArray &hash : header.covers)
        covers.append(toHex(hash));
    QJsonArray clients;
    for (const std::uint64_t client : header.clients)
        clients.append(QString::number(client));
    const QJsonObject object{
        {"board", header.board.toString(QUuid::WithoutBraces)},
        {"kind", header.kind == ContentKind::Snapshot ? "snapshot" : "changes"},
        {"device", toHex(header.certificate.deviceKey)},
        {"certificate", QJsonObject{{"payload", base64(header.certificate.payload)},
                                    {"signature", base64(header.certificate.signature)}}},
        {"covers", covers},
        {"clients", clients},
        {"stateVector", base64(header.stateVector)}};
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ContentKind parseKind(const QString &kind) {
    if (kind == QStringLiteral("snapshot"))
        return ContentKind::Snapshot;
    if (kind == QStringLiteral("changes"))
        return ContentKind::Changes;
    throw FileError("the content kind is unknown");
}

std::vector<std::uint64_t> parseClients(const QJsonArray &array) {
    std::vector<std::uint64_t> clients;
    for (const QJsonValue &value : array) {
        bool ok = false;
        clients.push_back(value.toString().toULongLong(&ok));
        if (!ok)
            throw FileError("a client id is malformed");
    }
    return clients;
}

ContentHeader parseHeader(const QByteArray &json) {
    const QJsonObject object = parseObject(json, "the header");
    ContentHeader header;
    header.board = QUuid::fromString(object["board"].toString());
    if (header.board.isNull())
        throw FileError("the header names no board");
    header.kind = parseKind(object["kind"].toString());
    const QByteArray device = hexField(object, "device", signingKeySize);
    const QJsonObject certificate = object["certificate"].toObject();
    const std::optional<DeviceCertificate> checked = verifyDeviceCertificate(
        base64Field(certificate, "payload"), base64Field(certificate, "signature"), device);
    if (!checked)
        throw FileError("the device certificate is invalid");
    header.certificate = *checked;
    for (const QJsonValue &value : object["covers"].toArray())
        header.covers.push_back(hexField(QJsonObject{{"hash", value}}, "hash", hashSize));
    header.clients = parseClients(object["clients"].toArray());
    header.stateVector = base64Field(object, "stateVector");
    return header;
}

QByteArray readPart(QDataStream &stream, quint32 limit, const char *what) {
    quint32 size = 0;
    stream >> size;
    if (stream.status() != QDataStream::Ok || size > limit)
        throw FileError(std::string(what) + " is too large or cut short");
    QByteArray part(static_cast<qsizetype>(size), '\0');
    if (stream.readRawData(part.data(), static_cast<int>(size)) != static_cast<int>(size))
        throw FileError(std::string(what) + " is cut short");
    return part;
}

} // namespace

QByteArray sealContent(const ContentHeader &header, const QByteArray &body,
                       const Signer &deviceSigner) {
    if (header.board.isNull() || header.certificate.deviceKey.size() != signingKeySize)
        throw std::invalid_argument("Content needs a board and a device certificate");
    QByteArray file = magic;
    QDataStream stream(&file, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    const QByteArray json = headerJson(header);
    stream << static_cast<quint32>(json.size());
    stream.writeRawData(json.constData(), static_cast<int>(json.size()));
    stream << static_cast<quint32>(body.size());
    stream.writeRawData(body.constData(), static_cast<int>(body.size()));
    return file + deviceSigner(file);
}

SignedContent openContent(const QByteArray &file) {
    if (!file.startsWith(magic) || file.size() < magic.size() + 8 + signatureSize)
        throw FileError("this is not a PeerChalk content file");
    const QByteArray signedPart = file.first(file.size() - signatureSize);
    QDataStream stream(signedPart.sliced(magic.size()));
    stream.setByteOrder(QDataStream::BigEndian);
    const QByteArray json = readPart(stream, maxHeaderBytes, "the header");
    const QByteArray body = readPart(stream, maxBodyBytes, "the content");
    if (!stream.atEnd())
        throw FileError("the file has extra bytes");
    SignedContent content{parseHeader(json), body};
    if (!verifySignature(content.header.certificate.deviceKey, signedPart,
                         file.last(signatureSize)))
        throw FileError("the signature does not match");
    return content;
}

} // namespace peerchalk::repository
