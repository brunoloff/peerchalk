#pragma once

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <functional>
#include <optional>
#include <vector>

namespace peerchalk {

// Signs bytes with a user's identity and returns the 64-byte signature.
using Signer = std::function<QByteArray(const QByteArray &)>;

// True when signature is a valid signature of message by the 32-byte signingKey.
[[nodiscard]] bool verifySignature(const QByteArray &signingKey, const QByteArray &message,
                                   const QByteArray &signature);

struct Member {
    QByteArray key; // user signing key
    QString name;
};

enum class ResourceKind { Project, Board };

// Who may access a project or a board, decided by its single admin. A board also lists
// the projects it belongs to; members of those projects may access it too. A deleted
// manifest is a tombstone telling peers the admin removed the resource.
struct Manifest {
    ResourceKind kind{ResourceKind::Project};
    QUuid id;
    QString name;
    QByteArray admin;
    quint64 version{1};
    std::vector<Member> members;
    std::vector<QUuid> projects; // boards only
    bool deleted{false};

    // The admin or a member.
    [[nodiscard]] bool includes(const QByteArray &userKey) const;
};

// A manifest together with the exact bytes its admin signed.
struct SignedManifest {
    Manifest manifest;
    QByteArray payload;
    QByteArray signature;
};

// Signs a manifest; the admin must be the signer's user. Throws std::invalid_argument
// for a manifest with a null id, an empty name or an admin key of the wrong size.
[[nodiscard]] SignedManifest signManifest(const Manifest &manifest, const Signer &signer);

// Checks a received manifest: it must parse, be of the expected kind and id, name the
// expected admin, and be signed by that admin. Returns nullopt otherwise.
[[nodiscard]] std::optional<SignedManifest> verifyManifest(const QByteArray &payload,
                                                           const QByteArray &signature,
                                                           ResourceKind kind, const QUuid &id,
                                                           const QByteArray &admin);

// Like verifyManifest, for a manifest whose id and admin are not known yet (a board
// learned through a project); the caller must then check its admin's authority.
[[nodiscard]] std::optional<SignedManifest> verifyAnyManifest(const QByteArray &payload,
                                                              const QByteArray &signature);

// A user's statement that a device (a p2pdc endpoint key) acts for them.
struct DeviceCertificate {
    QByteArray userKey;
    QByteArray deviceKey;
    QString deviceName;
    QByteArray payload;
    QByteArray signature;
};

[[nodiscard]] DeviceCertificate certifyDevice(const QByteArray &userKey,
                                              const QByteArray &deviceKey,
                                              const QString &deviceName, const Signer &signer);
// Checks that the certificate is signed by its user key and names deviceKey.
[[nodiscard]] std::optional<DeviceCertificate> verifyDeviceCertificate(const QByteArray &payload,
                                                                       const QByteArray &signature,
                                                                       const QByteArray &deviceKey);

} // namespace peerchalk
