#include "library/codes.hpp"
#include "library/files.hpp"
#include "ui/account_manager.hpp"
#include "ui/library.hpp"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

const QString passphrase = QStringLiteral("correct horse battery");

class AccountManagerTest : public ::testing::Test {
  protected:
    [[nodiscard]] QUrl fileUrl() const {
        return QUrl::fromLocalFile(QDir(root_.path()).filePath(QStringLiteral("a.peerchalk")));
    }
    QTemporaryDir root_;
    AccountManager accounts_{root_.path()};
};

TEST_F(AccountManagerTest, OpeningAnAccountSignsItInAndAsksTheApplication) {
    QSignalSpy opened(&accounts_, &AccountManager::openRequested);
    const QString id = accounts_.createAccount(QStringLiteral(" Ana "));
    ASSERT_FALSE(id.isEmpty());
    accounts_.openAccount(id);
    EXPECT_EQ(opened.size(), 1);
    EXPECT_EQ(accounts_.currentAccountId(), id);
    EXPECT_EQ(accounts_.accounts().front().toMap()[QStringLiteral("name")], QStringLiteral("Ana"));
}

TEST_F(AccountManagerTest, ReportsFailuresInsteadOfThrowing) {
    QSignalSpy errors(&accounts_, &AccountManager::errorOccurred);
    EXPECT_TRUE(accounts_.linkAccount(QStringLiteral("not a code")).isEmpty());
    EXPECT_FALSE(
        accounts_.prepareExport(QStringLiteral("missing"), QStringLiteral("people"), passphrase));
    EXPECT_EQ(errors.size(), 2);
}

TEST_F(AccountManagerTest, SavingABackupRecordsItsDateAndCanBeRestored) {
    const QString id = accounts_.createAccount(QStringLiteral("Ana"));
    accounts_.openAccount(id);
    EXPECT_FALSE(accounts_.currentLastBackup().isValid());
    ASSERT_TRUE(accounts_.prepareExport(id, QStringLiteral("everything"), passphrase));
    EXPECT_TRUE(accounts_.exportFileName().startsWith(QStringLiteral("peerchalk-ana-everything-")));
    accounts_.saveExport(fileUrl());
    EXPECT_TRUE(accounts_.currentLastBackup().isValid());
    QSignalSpy ready(&accounts_, &AccountManager::importFileReady);
    accounts_.loadImportFile(fileUrl());
    EXPECT_EQ(ready.size(), 1);
    EXPECT_TRUE(accounts_.importAccount(QStringLiteral("the wrong passphrase")).isEmpty());
    EXPECT_EQ(accounts_.importAccount(passphrase), id); // merged into the same account
}

TEST_F(AccountManagerTest, RemembersABoardLinkUntilItIsTaken) {
    const QUuid id = QUuid::createUuid();
    const QString link = encodeBoardLink({id, QStringLiteral("Sketch"), QByteArray(32, 'c'),
                                          QByteArray(32, 'k'), QByteArray(32, 't')},
                                         QStringLiteral("https://a.example"));
    accounts_.setPendingBoardLink(link);
    EXPECT_TRUE(accounts_.hasPendingBoardLink());
    EXPECT_EQ(accounts_.pendingBoardName(), QStringLiteral("Sketch"));
    EXPECT_EQ(accounts_.takePendingBoardLink(), link);
    EXPECT_FALSE(accounts_.hasPendingBoardLink());
    accounts_.setPendingBoardLink(QStringLiteral("https://a.example/"));
    EXPECT_FALSE(accounts_.hasPendingBoardLink()); // an ordinary address is not a link
}

TEST_F(AccountManagerTest, ChangingWhetherToReopenTheLastAccountIsAnnouncedOnce) {
    QSignalSpy changed(&accounts_, &AccountManager::opensLastAccountChanged);
    accounts_.setOpensLastAccount(true);
    accounts_.setOpensLastAccount(true);
    EXPECT_TRUE(accounts_.opensLastAccount());
    EXPECT_EQ(changed.size(), 1);
    EXPECT_TRUE(AccountManager(root_.path()).opensLastAccount());
}

TEST_F(AccountManagerTest, TurningOffUpdateChecksIsAnnouncedOnce) {
    QSignalSpy changed(&accounts_, &AccountManager::checksForUpdatesChanged);
    EXPECT_TRUE(accounts_.checksForUpdates());
    accounts_.setChecksForUpdates(false);
    accounts_.setChecksForUpdates(false);
    EXPECT_EQ(changed.size(), 1);
    EXPECT_FALSE(AccountManager(root_.path()).checksForUpdates());
}

TEST(SystemFromUserAgent, NamesComputersAndLeavesOutPhones) {
    EXPECT_EQ(systemFromUserAgent(QStringLiteral(
                  "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0")),
              QStringLiteral("Linux"));
    EXPECT_EQ(systemFromUserAgent(QStringLiteral(
                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/140.0")),
              QStringLiteral("Windows"));
    EXPECT_EQ(systemFromUserAgent(QStringLiteral(
                  "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15")),
              QStringLiteral("macOS"));
    EXPECT_EQ(systemFromUserAgent(QStringLiteral(
                  "Mozilla/5.0 (Linux; Android 15; Pixel 9) AppleWebKit/537.36 Mobile Safari")),
              QString());
    EXPECT_EQ(systemFromUserAgent(QStringLiteral(
                  "Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) Mobile/15E148")),
              QString());
    EXPECT_EQ(systemFromUserAgent(QStringLiteral("Mozilla/5.0 (X11; CrOS x86_64 16002.0.0)")),
              QString());
    EXPECT_EQ(systemFromUserAgent(QString()), QString());
}

TEST_F(AccountManagerTest, KnownPeopleMoveBetweenAccounts) {
    const QString ana = accounts_.createAccount(QStringLiteral("Ana"));
    const QString ben = accounts_.createAccount(QStringLiteral("Ben"));
    const QString cleo = accounts_.createAccount(QStringLiteral("Cleo"));
    {
        Library library(accounts_.store().directory(ana));
        Library other(accounts_.store().directory(cleo));
        ASSERT_TRUE(library.addContact(other.contactCode()));
    }
    ASSERT_TRUE(accounts_.prepareExport(ana, QStringLiteral("people"), passphrase));
    accounts_.saveExport(fileUrl());
    accounts_.loadImportFile(fileUrl());
    Library library(accounts_.store().directory(ben));
    library.importPeople(accounts_.importPeople(passphrase));
    EXPECT_EQ(library.identityStore().contacts().size(), 1U);
}

} // namespace
} // namespace peerchalk
