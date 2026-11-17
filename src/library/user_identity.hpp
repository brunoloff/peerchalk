#pragma once

#include "library/signatures.hpp"

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>
#include <span>

namespace p2pdc {
class Identity;
}

namespace peerchalk {

// The keys behind one PeerChalk installation:
// - the user key (Ed25519) says who you are; access to projects and boards is granted to
//   it, and all of a user's devices share it (see transferCode);
// - the device key (Ed25519) and recipient secret (X25519) belong to this installation
//   only; p2pdc authenticates connections with them, and the user key certifies them.
// identity.json holds the user seed and device.json the device secrets, both owner-only
// and not encrypted. Both are created on first use. Failures throw std::runtime_error.
class UserIdentity {
  public:
    explicit UserIdentity(QString directory);
    ~UserIdentity();
    UserIdentity(const UserIdentity &) = delete;
    UserIdentity &operator=(const UserIdentity &) = delete;

    [[nodiscard]] QByteArray userKey() const { return userKey_; }
    [[nodiscard]] QByteArray deviceKey() const { return deviceKey_; }
    [[nodiscard]] QString deviceName() const { return deviceName_; }
    // A 32-byte secret derived from the user seed: the discovery capability through
    // which a user's own devices find each other.
    [[nodiscard]] QByteArray personalCapability() const { return personalCapability_; }

    // Signs with the user key.
    [[nodiscard]] QByteArray sign(const QByteArray &message) const;
    [[nodiscard]] Signer signer() const;
    [[nodiscard]] DeviceCertificate deviceCertificate() const;

    // Calls use(signingSeed, recipientSecret) with this device's secrets in buffers that
    // are wiped afterwards, e.g. to create a p2pdc node.
    void withDeviceSecrets(const std::function<void(std::span<const std::byte>,
                                                    std::span<const std::byte>)> &use) const;

    // "peerchalk-identity:1:..." carrying the user seed and display name: secret, for
    // moving the identity to another device.
    [[nodiscard]] QString transferCode(const QString &displayName) const;
    // Replaces this installation's user key with the one in a transfer code and returns
    // the display name it carries; the device key is kept. Throws std::invalid_argument
    // for anything that is not a transfer code.
    QString adopt(const QString &transferCode);

  private:
    void loadUser();
    void loadDevice();
    void useUserSeed(const QByteArray &seed);

    QString directory_;
    std::unique_ptr<p2pdc::Identity> user_;
    QByteArray userKey_;
    QByteArray personalCapability_;
    QByteArray deviceKey_;
    QString deviceName_;
};

} // namespace peerchalk
