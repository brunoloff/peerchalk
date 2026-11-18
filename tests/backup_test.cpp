#include "library/backup.hpp"
#include "library/files.hpp"

#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

const QString passphrase = QStringLiteral("correct horse battery");
const QString boardFile = QStringLiteral("boards/00000000-0000-4000-8000-000000000001.yrs");

class BackupTest : public ::testing::Test {
  protected:
    void SetUp() override {
        for (const QString &name :
             {QStringLiteral("identity.json"), QStringLiteral("device.json"),
              QStringLiteral("profile.json"), QStringLiteral("contacts.json"),
              QStringLiteral("workspace.json"), QStringLiteral("network.json"), boardFile})
            writeFile(QDir(dir_.path()).filePath(name), name.toUtf8(), true);
    }
    [[nodiscard]] QByteArray backup(BackupKind kind) const {
        return createBackup(dir_.path(), kind, passphrase);
    }
    QTemporaryDir dir_;
};

TEST_F(BackupTest, EverythingRoundTripsWithoutDeviceSecretsOrSettings) {
    const BackupContents contents = openBackup(backup(BackupKind::Everything), passphrase);
    EXPECT_EQ(contents.kind, BackupKind::Everything);
    EXPECT_TRUE(contents.created.isValid());
    EXPECT_EQ(contents.files.size(), 5U);
    EXPECT_EQ(contents.files.at(boardFile), boardFile.toUtf8());
    EXPECT_FALSE(contents.files.contains(QStringLiteral("device.json")));
    EXPECT_FALSE(contents.files.contains(QStringLiteral("network.json")));
}

TEST_F(BackupTest, NarrowerKindsHoldOnlyTheirFiles) {
    EXPECT_EQ(openBackup(backup(BackupKind::Identity), passphrase).files.size(), 3U);
    const BackupContents people = openBackup(backup(BackupKind::People), passphrase);
    ASSERT_EQ(people.files.size(), 1U);
    EXPECT_TRUE(people.files.contains(QStringLiteral("contacts.json")));
}

TEST_F(BackupTest, ContentsAreNotReadableInTheFile) {
    EXPECT_FALSE(backup(BackupKind::Everything).contains("identity.json"));
}

TEST_F(BackupTest, WrongPassphraseAndTamperingAreRefused) {
    QByteArray file = backup(BackupKind::People);
    EXPECT_THROW(static_cast<void>(openBackup(file, QStringLiteral("another passphrase"))),
                 BackupAccessError);
    file[file.size() - 1] = static_cast<char>(file.back() ^ 1);
    EXPECT_THROW(static_cast<void>(openBackup(file, passphrase)), BackupAccessError);
    QByteArray header = backup(BackupKind::People);
    header[8] = static_cast<char>(header[8] ^ 1); // authenticated even though not secret
    EXPECT_THROW(static_cast<void>(openBackup(header, passphrase)), BackupAccessError);
}

TEST_F(BackupTest, RefusesOtherFilesAndHostileSettings) {
    EXPECT_THROW(static_cast<void>(openBackup(QByteArrayLiteral("not a backup"), passphrase)),
                 std::invalid_argument);
    QByteArray file = backup(BackupKind::People);
    file[15] = static_cast<char>(0x7f); // an enormous memory requirement
    EXPECT_THROW(static_cast<void>(openBackup(file, passphrase)), std::invalid_argument);
}

TEST_F(BackupTest, RefusesShortPassphrases) {
    EXPECT_THROW(static_cast<void>(
                     createBackup(dir_.path(), BackupKind::People, QStringLiteral("too short"))),
                 std::invalid_argument);
}

TEST_F(BackupTest, WritesItsFilesIntoAFolder) {
    QTemporaryDir target;
    writeBackupFiles(openBackup(backup(BackupKind::Everything), passphrase), target.path());
    EXPECT_EQ(readFile(QDir(target.path()).filePath(boardFile)), boardFile.toUtf8());
}

} // namespace
} // namespace peerchalk
