#include "library/signatures.hpp"
#include "library/user_identity.hpp"

#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

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

} // namespace
} // namespace peerchalk
