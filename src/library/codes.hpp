#pragma once

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <optional>

namespace peerchalk {

// Sizes of the keys and secrets that codes carry.
inline constexpr qsizetype signingKeySize = 32;
inline constexpr qsizetype capabilitySize = 32;
// Longest name, in UTF-8 bytes, that a code may carry.
inline constexpr qsizetype maxCodeNameBytes = 200;

// Everything needed to join a project: its identity, name and the secret that lets
// members find each other. Whoever holds an invite can join, so it is shared privately.
struct Invite {
    QUuid project;
    QString name;
    QByteArray capability;
};

// A person's public signing key and the name they chose; safe to share openly.
struct ContactCard {
    QByteArray signingKey;
    QString name;
};

// Text forms people copy and paste: "peerchalk-invite:1:" or "peerchalk-contact:1:"
// followed by unpadded base64url. Encoding throws std::invalid_argument for wrong key
// sizes, a null project or empty names; names longer than maxCodeNameBytes are cut at
// a character boundary. Decoding returns nullopt for anything malformed and ignores
// surrounding whitespace.
[[nodiscard]] QString encodeInvite(const Invite &invite);
[[nodiscard]] std::optional<Invite> decodeInvite(const QString &text);
[[nodiscard]] QString encodeContactCard(const ContactCard &card);
[[nodiscard]] std::optional<ContactCard> decodeContactCard(const QString &text);

// A signing key as upper-case hex in groups of four, e.g. "1A2B 3C4D ...".
[[nodiscard]] QString formatFingerprint(const QByteArray &signingKey);

} // namespace peerchalk
