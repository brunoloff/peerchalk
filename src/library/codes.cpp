#include "library/codes.hpp"

#include <QStringList>
#include <QUrl>

#include <stdexcept>

namespace peerchalk {
namespace {

const QString invitePrefix = QStringLiteral("peerchalk-invite:2:");
const QString contactPrefix = QStringLiteral("peerchalk-contact:1:");
constexpr qsizetype uuidSize = 16;
constexpr auto base64Options = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;

// UTF-8 bytes of name, cut so they fit maxCodeNameBytes without splitting a character.
QByteArray nameBytes(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        throw std::invalid_argument("A code needs a non-empty name");
    QByteArray bytes = trimmed.toUtf8();
    while (bytes.size() > maxCodeNameBytes) {
        qsizetype end = maxCodeNameBytes;
        while (end > 0 && (static_cast<unsigned char>(bytes[end]) & 0xC0U) == 0x80U)
            --end;
        bytes.truncate(end);
    }
    return bytes;
}

// The payload after prefix, or nullopt when the text is not such a code.
std::optional<QByteArray> payload(const QString &text, const QString &prefix) {
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith(prefix))
        return std::nullopt;
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        trimmed.mid(prefix.size()).toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        return std::nullopt;
    return decoded.decoded;
}

// The name at the end of a payload, or nullopt when it is empty, too long or not UTF-8.
std::optional<QString> trailingName(const QByteArray &bytes, qsizetype offset) {
    const QByteArray raw = bytes.mid(offset);
    if (raw.isEmpty() || raw.size() > maxCodeNameBytes || !QUtf8StringView(raw).isValidUtf8())
        return std::nullopt;
    const QString name = QString::fromUtf8(raw).trimmed();
    if (name.isEmpty())
        return std::nullopt;
    return name;
}

} // namespace

// Invite payload: kind byte (0 project, 1 board), id, capability, creator key, name.
QString encodeInvite(const Invite &invite) {
    if (invite.id.isNull() || invite.capability.size() != capabilitySize ||
        invite.creator.size() != signingKeySize)
        throw std::invalid_argument("An invite needs an id, a capability and a creator key");
    const char kind = invite.kind == Invite::Kind::Board ? 1 : 0;
    const QByteArray bytes = QByteArray(1, kind) + invite.id.toRfc4122() + invite.capability +
                             invite.creator + nameBytes(invite.name);
    return invitePrefix + QString::fromLatin1(bytes.toBase64(base64Options));
}

std::optional<Invite> decodeInvite(const QString &text) {
    constexpr qsizetype nameOffset = 1 + uuidSize + capabilitySize + signingKeySize;
    const std::optional<QByteArray> bytes = payload(text, invitePrefix);
    if (!bytes || bytes->size() <= nameOffset || (bytes->at(0) != 0 && bytes->at(0) != 1))
        return std::nullopt;
    const QUuid id = QUuid::fromRfc4122(bytes->mid(1, uuidSize));
    const std::optional<QString> name = trailingName(*bytes, nameOffset);
    if (id.isNull() || !name)
        return std::nullopt;
    return Invite{bytes->at(0) == 1 ? Invite::Kind::Board : Invite::Kind::Project, id, *name,
                  bytes->mid(1 + uuidSize, capabilitySize),
                  bytes->mid(1 + uuidSize + capabilitySize, signingKeySize)};
}

QString encodeContactCard(const ContactCard &card) {
    if (card.signingKey.size() != signingKeySize)
        throw std::invalid_argument("A contact card needs a 32-byte signing key");
    const QByteArray bytes = card.signingKey + nameBytes(card.name);
    return contactPrefix + QString::fromLatin1(bytes.toBase64(base64Options));
}

std::optional<ContactCard> decodeContactCard(const QString &text) {
    const std::optional<QByteArray> bytes = payload(text, contactPrefix);
    if (!bytes || bytes->size() <= signingKeySize)
        return std::nullopt;
    const std::optional<QString> name = trailingName(*bytes, signingKeySize);
    if (!name)
        return std::nullopt;
    return ContactCard{bytes->first(signingKeySize), *name};
}

QString encodeBoardLink(const BoardLink &link, const QString &siteUrl) {
    if (link.id.isNull() || link.capability.size() != capabilitySize ||
        link.creator.size() != signingKeySize || link.token.size() != linkTokenSize)
        throw std::invalid_argument("A board link needs an id, secrets and a creator key");
    QString site = siteUrl.trimmed();
    while (site.endsWith(QLatin1Char('/')))
        site.chop(1);
    const QByteArray bytes = link.capability + link.creator + link.token + nameBytes(link.name);
    return site + QStringLiteral("/board/") + link.id.toString(QUuid::WithoutBraces) +
           QLatin1Char('#') + QString::fromLatin1(bytes.toBase64(base64Options));
}

std::optional<QUuid> boardIdFromLink(const QString &url) {
    const QString path = QUrl(url.trimmed()).path();
    const qsizetype marker = path.lastIndexOf(QStringLiteral("/board/"));
    if (marker < 0)
        return std::nullopt;
    const QUuid id = QUuid::fromString(path.mid(marker + 7));
    if (id.isNull())
        return std::nullopt;
    return id;
}

std::optional<BoardLink> decodeBoardLink(const QString &url) {
    constexpr qsizetype nameOffset = capabilitySize + signingKeySize + linkTokenSize;
    const std::optional<QUuid> id = boardIdFromLink(url);
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        QUrl(url.trimmed()).fragment().toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!id || !decoded || decoded.decoded.size() <= nameOffset)
        return std::nullopt;
    const QByteArray &bytes = decoded.decoded;
    const std::optional<QString> name = trailingName(bytes, nameOffset);
    if (!name)
        return std::nullopt;
    return BoardLink{*id, *name, bytes.first(capabilitySize),
                     bytes.mid(capabilitySize, signingKeySize),
                     bytes.mid(capabilitySize + signingKeySize, linkTokenSize)};
}

QString formatFingerprint(const QByteArray &signingKey) {
    const QString hex = QString::fromLatin1(signingKey.toHex()).toUpper();
    QStringList groups;
    for (qsizetype i = 0; i < hex.size(); i += 4)
        groups.append(hex.mid(i, 4));
    return groups.join(QLatin1Char(' '));
}

} // namespace peerchalk
