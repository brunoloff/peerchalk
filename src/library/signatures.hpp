#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

namespace peerchalk {

// Signs bytes with a user's identity and returns the 64-byte signature.
using Signer = std::function<QByteArray(const QByteArray &)>;

// True when signature is a valid signature of message by the 32-byte signingKey.
[[nodiscard]] bool verifySignature(const QByteArray &signingKey, const QByteArray &message,
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
