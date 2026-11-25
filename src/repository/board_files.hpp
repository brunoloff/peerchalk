#pragma once

#include "library/access_log.hpp"
#include "library/signatures.hpp"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <optional>
#include <vector>

namespace peerchalk::repository {

// The small files of a repository board besides its content (signed_content.hpp). Readers
// throw FileError for anything malformed.

inline constexpr int boardFormatVersion = 1;

// boards/<id>/board.json: written once by the board's creator.
struct BoardInfo {
    QUuid board;
    QByteArray creator; // user key; the access log's trust anchor
};
[[nodiscard]] QByteArray encodeBoardInfo(const BoardInfo &info);
[[nodiscard]] BoardInfo decodeBoardInfo(const QByteArray &file);

// boards/<id>/access/<sha256 of the payload>.op: one signed access entry.
[[nodiscard]] QByteArray encodeAccessFile(const SignedOp &op);
[[nodiscard]] SignedOp decodeAccessFile(const QByteArray &file);
[[nodiscard]] QString accessFileName(const SignedOp &op);

// boards/<id>/secrets/<epoch>/<signer>/<user>: the board's network secret for one epoch,
// sealed to one member's user key and signed by whoever sealed it (an owner, or the
// creator). Two owners who start the same epoch at once write different paths; everyone
// then uses the one from the lowest signer key that sealed to them.
struct BoardSecret {
    QUuid board;
    int epoch{0};
    QByteArray user;   // whom it is sealed to
    QByteArray sealed; // p2pdc::seal_to_signing_key(user, capability)
    QByteArray signer; // user key
};
[[nodiscard]] QByteArray encodeSecretFile(const BoardSecret &secret, const Signer &signer);
// Checks the signature by secret.signer; whether the signer may seal is the caller's question.
[[nodiscard]] BoardSecret decodeSecretFile(const QByteArray &file);
[[nodiscard]] QString secretFileName(const BoardSecret &secret);
// Seals a capability to a user key; throws std::runtime_error when the key is unusable.
[[nodiscard]] QByteArray sealCapability(const QByteArray &userKey, const QByteArray &capability);

// people/<user>.json: a person's name and devices, signed by their user key.
struct Person {
    QByteArray userKey;
    QString name;
    QString email; // optional, for commit trailers
    std::vector<DeviceCertificate> devices;
};
[[nodiscard]] QByteArray encodePerson(const Person &person, const Signer &userSigner);
[[nodiscard]] Person decodePerson(const QByteArray &file);

} // namespace peerchalk::repository
