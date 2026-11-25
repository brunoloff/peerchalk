#include "library/signatures.hpp"
#include "library/user_identity.hpp"

#include <p2pdc/recipient_crypto.h>

#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

QByteArray sealForTest(const QByteArray &userKey, const QByteArray &secret) {
    const p2pdc::CryptoResult<p2pdc::Bytes> sealed =
        p2pdc::seal_to_signing_key({reinterpret_cast<const std::byte *>(userKey.constData()), 32},
                                   {reinterpret_cast<const std::byte *>(secret.constData()),
                                    static_cast<std::size_t>(secret.size())},
                                   64);
    const auto &bytes = std::get<p2pdc::Bytes>(sealed);
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size())};
}

class SignaturesTest : public ::testing::Test {
  protected:
    QTemporaryDir aliceDir_;
    QTemporaryDir bobDir_;
    UserIdentity alice_{aliceDir_.path()};
    UserIdentity bob_{bobDir_.path()};
};

TEST_F(SignaturesTest, DeviceCertificatesBindDeviceToUser) {
    const DeviceCertificate certificate = alice_.deviceCertificate();
    const std::optional<DeviceCertificate> verified =
        verifyDeviceCertificate(certificate.payload, certificate.signature, alice_.deviceKey());
    ASSERT_TRUE(verified.has_value());
    EXPECT_EQ(verified->userKey, alice_.userKey());
    EXPECT_FALSE(
        verifyDeviceCertificate(certificate.payload, certificate.signature, bob_.deviceKey()));
}

TEST_F(SignaturesTest, LinkedDevicesShareTheUserButNotTheDevice) {
    QTemporaryDir otherDir;
    UserIdentity laptop(otherDir.path());
    EXPECT_EQ(laptop.adopt(alice_.transferCode(QStringLiteral("Alice"))), QStringLiteral("Alice"));
    EXPECT_EQ(laptop.userKey(), alice_.userKey());
    EXPECT_NE(laptop.deviceKey(), alice_.deviceKey());
    EXPECT_EQ(laptop.personalCapability(), alice_.personalCapability());
    EXPECT_THROW(laptop.adopt(QStringLiteral("nonsense")), std::invalid_argument);
}

TEST_F(SignaturesTest, DevicesSignForThemselvesAndKeepTheirOwnFolder) {
    const QByteArray message("change file");
    EXPECT_TRUE(verifySignature(alice_.deviceKey(), message, alice_.signAsDevice(message)));
    EXPECT_FALSE(verifySignature(alice_.userKey(), message, alice_.signAsDevice(message)));
    QTemporaryDir cloneDir;
    const UserIdentity clone(aliceDir_.path(), cloneDir.path()); // same user, another clone
    EXPECT_EQ(clone.userKey(), alice_.userKey());
    EXPECT_NE(clone.deviceKey(), alice_.deviceKey());
    EXPECT_EQ(UserIdentity(aliceDir_.path(), cloneDir.path()).deviceKey(), clone.deviceKey());
}

TEST_F(SignaturesTest, SealedSecretsOpenOnlyForTheirUser) {
    QTemporaryDir cloneDir;
    const UserIdentity clone(aliceDir_.path(), cloneDir.path());
    const QByteArray sealed = sealForTest(alice_.userKey(), QByteArray(32, 's'));
    EXPECT_EQ(clone.openSealed(sealed, 64), QByteArray(32, 's')); // any of the user's devices
    EXPECT_THROW((void)bob_.openSealed(sealed, 64), std::runtime_error);
    EXPECT_THROW((void)alice_.openSealed(sealed, 16), std::runtime_error); // too large
}

} // namespace
} // namespace peerchalk
