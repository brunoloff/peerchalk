#include "library/manifest.hpp"
#include "library/user_identity.hpp"

#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

class ManifestTest : public ::testing::Test {
  protected:
    QTemporaryDir aliceDir_;
    QTemporaryDir bobDir_;
    UserIdentity alice_{aliceDir_.path()};
    UserIdentity bob_{bobDir_.path()};

    [[nodiscard]] Manifest projectBy(const UserIdentity &admin) const {
        return {ResourceKind::Project,
                id_,
                QStringLiteral("Thesis"),
                admin.userKey(),
                3,
                {{bob_.userKey(), QStringLiteral("Bob")}},
                {},
                false};
    }
    QUuid id_ = QUuid::createUuid();
};

TEST_F(ManifestTest, SignedManifestVerifies) {
    const SignedManifest signedManifest = signManifest(projectBy(alice_), alice_.signer());
    const std::optional<SignedManifest> verified =
        verifyManifest(signedManifest.payload, signedManifest.signature, ResourceKind::Project, id_,
                       alice_.userKey());
    ASSERT_TRUE(verified.has_value());
    EXPECT_EQ(verified->manifest.version, 3U);
    EXPECT_TRUE(verified->manifest.includes(bob_.userKey()));
    EXPECT_TRUE(verified->manifest.includes(alice_.userKey()));
}

TEST_F(ManifestTest, TamperedOrMisattributedManifestsFail) {
    SignedManifest signedManifest = signManifest(projectBy(alice_), alice_.signer());
    EXPECT_FALSE(verifyManifest(signedManifest.payload, signedManifest.signature,
                                ResourceKind::Board, id_, alice_.userKey()));
    EXPECT_FALSE(verifyManifest(signedManifest.payload, signedManifest.signature,
                                ResourceKind::Project, id_, bob_.userKey()));
    signedManifest.payload.replace("Thesis", "Thesiz");
    EXPECT_FALSE(verifyAnyManifest(signedManifest.payload, signedManifest.signature));
}

TEST_F(ManifestTest, ManifestSignedByAnotherUserIsRejected) {
    // Bob signs a manifest claiming Alice is the admin.
    const SignedManifest forged = signManifest(projectBy(alice_), bob_.signer());
    EXPECT_FALSE(verifyAnyManifest(forged.payload, forged.signature));
}

TEST_F(ManifestTest, DeviceCertificatesBindDeviceToUser) {
    const DeviceCertificate certificate = alice_.deviceCertificate();
    const std::optional<DeviceCertificate> verified =
        verifyDeviceCertificate(certificate.payload, certificate.signature, alice_.deviceKey());
    ASSERT_TRUE(verified.has_value());
    EXPECT_EQ(verified->userKey, alice_.userKey());
    EXPECT_FALSE(
        verifyDeviceCertificate(certificate.payload, certificate.signature, bob_.deviceKey()));
}

TEST_F(ManifestTest, LinkedDevicesShareTheUserButNotTheDevice) {
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
