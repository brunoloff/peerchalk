#pragma once

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <optional>

namespace peerchalk {

// Sizes of the keys and secrets that codes carry.
inline constexpr qsizetype signingKeySize = 32;
inline constexpr qsizetype capabilitySize = 32;
inline constexpr qsizetype linkTokenSize = 32;
// Longest name, in UTF-8 bytes, that a code may carry.
inline constexpr qsizetype maxCodeNameBytes = 200;

// Everything needed to join a project or a single board: what it is, its name, the
// secret that lets its people find each other, and its creator's key, from which its
// signed access changes are checked. Only people let in get any content, but the
// secret should stay private.
struct Invite {
    enum class Kind { Project, Board };
    Kind kind{Kind::Project};
    QUuid id;
    QString name;
    QByteArray capability;
    QByteArray creator;
};

// A link anyone can open to join one board. Everything secret follows the # of the
// address, which browsers do not send to the website:
// <site>/board/<board id>#<unpadded base64url of capability, creator, token, name>
struct BoardLink {
    QUuid id;
    QString name;
    QByteArray capability;
    QByteArray creator;
    QByteArray token; // proves to an owner that the holder was given the link
};

// A person's public signing key and the name they chose; safe to share openly.
struct ContactCard {
    QByteArray signingKey;
    QString name;
};

// Text forms people copy and paste: "peerchalk-invite:2:" or "peerchalk-contact:1:"
// followed by unpadded base64url. Encoding throws std::invalid_argument for wrong key
// sizes, a null id or empty names; names longer than maxCodeNameBytes are cut at
// a character boundary. Decoding returns nullopt for anything malformed and ignores
// surrounding whitespace.
[[nodiscard]] QString encodeInvite(const Invite &invite);
[[nodiscard]] std::optional<Invite> decodeInvite(const QString &text);
[[nodiscard]] QString encodeContactCard(const ContactCard &card);
[[nodiscard]] std::optional<ContactCard> decodeContactCard(const QString &text);

[[nodiscard]] QString encodeBoardLink(const BoardLink &link, const QString &siteUrl);
// Nullopt unless the address holds a complete, well-formed link.
[[nodiscard]] std::optional<BoardLink> decodeBoardLink(const QString &url);
// The board an address points at, with or without its secret part.
[[nodiscard]] std::optional<QUuid> boardIdFromLink(const QString &url);

// A signing key as upper-case hex in groups of four, e.g. "1A2B 3C4D ...".
[[nodiscard]] QString formatFingerprint(const QByteArray &signingKey);

} // namespace peerchalk
