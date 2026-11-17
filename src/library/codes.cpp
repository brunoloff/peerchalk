#include "library/codes.hpp"

#include <QStringList>

#include <stdexcept>

namespace peerchalk {
namespace {

const QString invitePrefix = QStringLiteral("peerchalk-invite:1:");
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

QString encodeInvite(const Invite &invite) {
    if (invite.project.isNull() || invite.capability.size() != capabilitySize)
        throw std::invalid_argument("An invite needs a project and a 32-byte capability");
    const QByteArray bytes =
        invite.project.toRfc4122() + invite.capability + nameBytes(invite.name);
    return invitePrefix + QString::fromLatin1(bytes.toBase64(base64Options));
}

std::optional<Invite> decodeInvite(const QString &text) {
    const std::optional<QByteArray> bytes = payload(text, invitePrefix);
    if (!bytes || bytes->size() <= uuidSize + capabilitySize)
        return std::nullopt;
    const QUuid project = QUuid::fromRfc4122(bytes->first(uuidSize));
    const std::optional<QString> name = trailingName(*bytes, uuidSize + capabilitySize);
    if (project.isNull() || !name)
        return std::nullopt;
    return Invite{project, *name, bytes->mid(uuidSize, capabilitySize)};
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

QString formatFingerprint(const QByteArray &signingKey) {
    const QString hex = QString::fromLatin1(signingKey.toHex()).toUpper();
    QStringList groups;
    for (qsizetype i = 0; i < hex.size(); i += 4)
        groups.append(hex.mid(i, 4));
    return groups.join(QLatin1Char(' '));
}

} // namespace peerchalk
