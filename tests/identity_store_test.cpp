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

TEST_F(IdentityStoreTest, RemembersRecentCustomColours) {
    {
        IdentityStore store(dir_.path(), QStringLiteral("Me"));
        for (int i = 0; i < IdentityStore::maxCustomColors + 2; ++i)
            store.addCustomColor(QStringLiteral("#0000%1").arg(i, 2, 10, QLatin1Char('0')));
        store.addCustomColor(QStringLiteral("#abcdef"));
        store.addCustomColor(QStringLiteral("#000005")); // already there: moves to the front
    }
    const IdentityStore reloaded(dir_.path(), QStringLiteral("Me"));
    const QStringList &colors = reloaded.customColors();
    ASSERT_EQ(colors.size(), IdentityStore::maxCustomColors);
    EXPECT_EQ(colors[0], QStringLiteral("#000005"));
    EXPECT_EQ(colors[1], QStringLiteral("#ABCDEF"));
    EXPECT_EQ(colors.count(QStringLiteral("#000005")), 1);
    EXPECT_FALSE(colors.contains(QStringLiteral("#000000"))); // the oldest fell off
}

TEST_F(IdentityStoreTest, RejectsMalformedCustomColours) {
    IdentityStore store(dir_.path(), QStringLiteral("Me"));
    for (const char *bad : {"red", "#12345", "#1234567", "#GGGGGG", "123456#"})
        EXPECT_THROW(store.addCustomColor(QString::fromLatin1(bad)), std::invalid_argument) << bad;
    EXPECT_TRUE(store.customColors().isEmpty());
}

// Group and other permission bits exist only on Unix; Windows keeps a user's data private through
// the access rules of the user's profile folder, which Qt does not report here.
#ifndef Q_OS_WIN
TEST_F(IdentityStoreTest, SecretsAreReadableOnlyByTheOwner) {
    const IdentityStore store(dir_.path(), QStringLiteral("Me"));
    const QFile::Permissions permissions =
        QFile::permissions(QDir(dir_.path()).filePath(QStringLiteral("identity.json")));
    EXPECT_FALSE(permissions & (QFile::ReadGroup | QFile::ReadOther));
}
#endif

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
