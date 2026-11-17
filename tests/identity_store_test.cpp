#include "library/files.hpp"
#include "library/identity_store.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

class IdentityStoreTest : public ::testing::Test {
  protected:
    QTemporaryDir dir_;
};

TEST_F(IdentityStoreTest, CreatesAnIdentityOnceAndKeepsIt) {
    QByteArray key;
    {
        IdentityStore store(dir_.path(), QStringLiteral("Bruno"));
        key = store.signingKey();
        EXPECT_EQ(key.size(), signingKeySize);
        EXPECT_EQ(store.displayName(), QStringLiteral("Bruno"));
        store.setDisplayName(QStringLiteral("B."));
        store.setColorIndex(3);
    }
    const IdentityStore reloaded(dir_.path(), QStringLiteral("Ignored"));
    EXPECT_EQ(reloaded.signingKey(), key);
    EXPECT_EQ(reloaded.displayName(), QStringLiteral("B."));
    EXPECT_EQ(reloaded.colorIndex(), 3);
}

TEST_F(IdentityStoreTest, SecretsAreReadableOnlyByTheOwner) {
    const IdentityStore store(dir_.path(), QStringLiteral("Me"));
    const QFile::Permissions permissions =
        QFile::permissions(QDir(dir_.path()).filePath(QStringLiteral("identity.json")));
    EXPECT_FALSE(permissions & (QFile::ReadGroup | QFile::ReadOther));
}

TEST_F(IdentityStoreTest, KeepsContactsAndTheirVerification) {
    const QByteArray anaKey(32, 'a');
    {
        IdentityStore store(dir_.path(), QStringLiteral("Me"));
        store.addContact({anaKey, QStringLiteral("Ana")});
        store.addContact({anaKey, QStringLiteral("Ana M.")});
        store.setVerification(anaKey, Verification::InPerson);
        ASSERT_EQ(store.contacts().size(), 1U);
    }
    const IdentityStore reloaded(dir_.path(), QStringLiteral("Me"));
    ASSERT_EQ(reloaded.contacts().size(), 1U);
    EXPECT_EQ(reloaded.contacts().front().name, QStringLiteral("Ana M."));
    EXPECT_EQ(reloaded.contacts().front().verification, Verification::InPerson);
}

TEST_F(IdentityStoreTest, RejectsYourOwnCardAndUnknownPeople) {
    IdentityStore store(dir_.path(), QStringLiteral("Me"));
    EXPECT_THROW(store.addContact(store.contactCard()), std::invalid_argument);
    EXPECT_THROW(store.setVerification(QByteArray(32, 'z'), Verification::Call),
                 std::invalid_argument);
    EXPECT_THROW(store.removeContact(QByteArray(32, 'z')), std::invalid_argument);
    EXPECT_THROW(store.setColorIndex(42), std::invalid_argument);
}

TEST_F(IdentityStoreTest, RemovesContacts) {
    IdentityStore store(dir_.path(), QStringLiteral("Me"));
    store.addContact({QByteArray(32, 'k'), QStringLiteral("Kofi")});
    store.removeContact(QByteArray(32, 'k'));
    EXPECT_TRUE(store.contacts().empty());
}

TEST_F(IdentityStoreTest, DamagedIdentityFailsLoudly) {
    writeJsonObject(QDir(dir_.path()).filePath(QStringLiteral("identity.json")),
                    {{QStringLiteral("signingSeed"), QStringLiteral("c2hvcnQ=")}}, 1, true);
    EXPECT_THROW(IdentityStore store(dir_.path(), QStringLiteral("Me")), std::runtime_error);
}

} // namespace
} // namespace peerchalk
