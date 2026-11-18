#include "library/account_store.hpp"
#include "library/files.hpp"
#include "library/identity_store.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace peerchalk {
namespace {

const QString passphrase = QStringLiteral("correct horse battery");

class AccountStoreTest : public ::testing::Test {
  protected:
    QTemporaryDir root_;
    AccountStore store_{root_.path()};
};

TEST_F(AccountStoreTest, StartsEmptyAndSignedOut) {
    EXPECT_TRUE(store_.accounts().empty());
    EXPECT_TRUE(store_.current().isEmpty());
    store_.migrateLegacyData(); // nothing to move
    EXPECT_TRUE(store_.accounts().empty());
}

TEST_F(AccountStoreTest, CreatedAccountsAreListedAndRemembered) {
    const QString ana = store_.create(QStringLiteral("Ana"));
    const QString ben = store_.create(QStringLiteral("Ben"));
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // timestamps have 1 ms steps
    store_.markOpened(ana);
    const auto accounts = AccountStore(root_.path()).accounts();
    ASSERT_EQ(accounts.size(), 2U);
    EXPECT_EQ(accounts[0].id, ana);
    EXPECT_EQ(accounts[0].name, QStringLiteral("Ana"));
    EXPECT_NE(accounts[0].userKey, accounts[1].userKey);
    store_.setCurrent(ben);
    EXPECT_EQ(store_.current(), ben);
    store_.setCurrent({});
    EXPECT_TRUE(store_.current().isEmpty());
}

TEST_F(AccountStoreTest, RemovingAnAccountDeletesItAndSignsItOut) {
    const QString id = store_.create(QStringLiteral("Ana"));
    const QString directory = store_.directory(id);
    store_.setCurrent(id);
    store_.remove(id);
    EXPECT_FALSE(QDir(directory).exists());
    EXPECT_TRUE(store_.current().isEmpty());
    EXPECT_THROW(store_.remove(id), std::invalid_argument);
}

TEST_F(AccountStoreTest, RejectsPathsAsAccountIds) {
    EXPECT_THROW(static_cast<void>(store_.directory(QStringLiteral("../.."))),
                 std::invalid_argument);
}

TEST_F(AccountStoreTest, MovesExistingDataIntoASignedInAccount) {
    QByteArray key;
    QUuid project;
    {
        IdentityStore identity(root_.path(), QStringLiteral("Ana"));
        key = identity.signingKey();
        Workspace workspace(root_.path(), key, identity.identity().signer());
        project = workspace.createProject(QStringLiteral("Thesis"), QStringLiteral("Ana"));
    }
    store_.migrateLegacyData();
    EXPECT_FALSE(QDir(root_.path()).exists(QStringLiteral("identity.json")));
    ASSERT_EQ(store_.accounts().size(), 1U);
    const AccountInfo account = store_.accounts().front();
    EXPECT_EQ(account.userKey, key);
    EXPECT_EQ(store_.current(), account.id);
    IdentityStore identity(store_.directory(account.id), QStringLiteral("x"));
    const Workspace workspace(store_.directory(account.id), key, identity.identity().signer());
    EXPECT_TRUE(workspace.hasProject(project));
}

TEST_F(AccountStoreTest, LinkingUsesTheSameUserWithANewDeviceAndOnlyOnce) {
    QTemporaryDir other;
    const IdentityStore phone(other.path(), QStringLiteral("Ana"));
    const QString code = phone.identity().transferCode(QStringLiteral("Ana"));
    const QString id = store_.link(code);
    const AccountInfo account = store_.accounts().front();
    EXPECT_EQ(account.id, id);
    EXPECT_EQ(account.userKey, phone.signingKey());
    EXPECT_EQ(account.name, QStringLiteral("Ana"));
    const IdentityStore laptop(store_.directory(id), QStringLiteral("x"));
    EXPECT_NE(laptop.identity().deviceKey(), phone.identity().deviceKey());
    EXPECT_THROW(static_cast<void>(store_.link(code)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(store_.link(QStringLiteral("nonsense"))), std::invalid_argument);
}

TEST_F(AccountStoreTest, RestoresABackupAsANewAccountWithANewDevice) {
    const QString original = store_.create(QStringLiteral("Ana"));
    const QString directory = store_.directory(original);
    QByteArray deviceKey;
    QUuid board;
    {
        IdentityStore identity(directory, QStringLiteral("x"));
        deviceKey = identity.identity().deviceKey();
        Workspace workspace(directory, identity.signingKey(), identity.identity().signer());
        const QUuid project = workspace.createProject(QStringLiteral("P"), QStringLiteral("Ana"));
        board = workspace.createBoard(QStringLiteral("B"), project, QStringLiteral("Ana"));
        workspace.saveBoardState(board, QByteArrayLiteral("state"));
    }
    const QByteArray file = createBackup(directory, BackupKind::Everything, passphrase);
    QTemporaryDir elsewhere;
    AccountStore restored(elsewhere.path());
    const ImportedAccount imported = restored.import(openBackup(file, passphrase));
    EXPECT_FALSE(imported.merged);
    const QString target = restored.directory(imported.id);
    IdentityStore identity(target, QStringLiteral("x"));
    EXPECT_EQ(identity.displayName(), QStringLiteral("Ana"));
    EXPECT_NE(identity.identity().deviceKey(), deviceKey);
    const Workspace workspace(target, identity.signingKey(), identity.identity().signer());
    EXPECT_EQ(workspace.boardState(board), QByteArrayLiteral("state"));
}

TEST_F(AccountStoreTest, ImportingAnAccountAlreadyHereMergesInsteadOfReplacing) {
    const QString id = store_.create(QStringLiteral("Ana"));
    const QString directory = store_.directory(id);
    const QByteArray file = createBackup(directory, BackupKind::Everything, passphrase);
    QUuid newer;
    {
        IdentityStore identity(directory, QStringLiteral("x"));
        Workspace workspace(directory, identity.signingKey(), identity.identity().signer());
        newer = workspace.createProject(QStringLiteral("Newer"), QStringLiteral("Ana"));
    }
    const ImportedAccount imported = store_.import(openBackup(file, passphrase));
    EXPECT_TRUE(imported.merged);
    EXPECT_EQ(imported.id, id);
    EXPECT_EQ(store_.accounts().size(), 1U);
    IdentityStore identity(directory, QStringLiteral("x"));
    const Workspace workspace(directory, identity.signingKey(), identity.identity().signer());
    EXPECT_TRUE(workspace.hasProject(newer));
}

TEST_F(AccountStoreTest, PeopleBackupsCannotBecomeAccounts) {
    const QString id = store_.create(QStringLiteral("Ana"));
    const QByteArray file = createBackup(store_.directory(id), BackupKind::People, passphrase);
    EXPECT_THROW(static_cast<void>(store_.import(openBackup(file, passphrase))),
                 std::invalid_argument);
}

} // namespace
} // namespace peerchalk
