#include "ui/library.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

class LibraryTest : public ::testing::Test {
  protected:
    QTemporaryDir dir_;
};

QString role(const RowModel *model, int row, const char *name) {
    const int roleId = model->roleNames().key(name);
    return model->data(model->index(row), roleId).toString();
}

TEST_F(LibraryTest, CreatingAProjectSelectsIt) {
    Library library(dir_.path());
    QSignalSpy changed(&library, &Library::currentProjectChanged);
    const QString id = library.createProject(QStringLiteral("Thesis"));
    EXPECT_EQ(library.currentProjectId(), id);
    EXPECT_EQ(library.currentProjectName(), QStringLiteral("Thesis"));
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(library.projects()->rowCount(), 1);
}

TEST_F(LibraryTest, BoardsAreListedNewestFirstAndFiltered) {
    Library library(dir_.path());
    library.createProject(QStringLiteral("P"));
    const QString first = library.createBoard(QStringLiteral("Reading list"));
    library.createBoard(QStringLiteral("Proof sketch"));
    library.saveBoardState(first, QByteArray("x"));
    EXPECT_EQ(role(library.boards(), 0, "name"), QStringLiteral("Reading list"));
    library.setBoardFilter(QStringLiteral("PROOF"));
    ASSERT_EQ(library.boards()->rowCount(), 1);
    EXPECT_EQ(role(library.boards(), 0, "name"), QStringLiteral("Proof sketch"));
}

TEST_F(LibraryTest, InvitesJoinAnotherLibrary) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library bob(other.path());
    const QString project = alice.createProject(QStringLiteral("Shared"));
    EXPECT_EQ(bob.joinInvite(alice.projectInvite(project)), project);
    EXPECT_EQ(bob.currentProjectName(), QStringLiteral("Shared"));
    EXPECT_TRUE(bob.currentProjectPending());
    EXPECT_FALSE(bob.currentProjectIsAdmin());
    EXPECT_TRUE(bob.projectInvite(project).isEmpty()); // only the admin invites
}

TEST_F(LibraryTest, AdminAddsAndRemovesMembersFromContacts) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library bob(other.path());
    const QString project = alice.createProject(QStringLiteral("P"));
    ASSERT_TRUE(alice.addContact(bob.contactCode()));
    alice.setProjectMember(project, bob.userKey(), true);
    QVariantList people = alice.projectPeople(project);
    ASSERT_EQ(people.size(), 2);
    EXPECT_TRUE(people[0].toMap()["admin"].toBool());
    EXPECT_TRUE(people[1].toMap()["member"].toBool());
    alice.setProjectMember(project, bob.userKey(), false);
    people = alice.projectPeople(project);
    EXPECT_FALSE(people[1].toMap()["member"].toBool());
}

TEST_F(LibraryTest, BoardsCanJoinMoreProjects) {
    Library alice(dir_.path());
    const QString first = alice.createProject(QStringLiteral("First"));
    const QString board = alice.createBoard(QStringLiteral("Board"));
    const QString second = alice.createProject(QStringLiteral("Second"));
    alice.setBoardInProject(board, second, true);
    EXPECT_EQ(alice.boards()->rowCount(), 1);
    alice.setCurrentProjectId(first);
    EXPECT_EQ(alice.boards()->rowCount(), 1);
}

TEST_F(LibraryTest, LinkingADeviceAdoptsTheIdentity) {
    QTemporaryDir other;
    Library phone(dir_.path());
    Library laptop(other.path());
    ASSERT_TRUE(laptop.canAdoptIdentity());
    ASSERT_TRUE(laptop.adoptIdentity(phone.identityTransferCode()));
    EXPECT_EQ(laptop.userKey(), phone.userKey());
    phone.createProject(QStringLiteral("Now not empty"));
    EXPECT_FALSE(phone.adoptIdentity(laptop.identityTransferCode()));
}

TEST_F(LibraryTest, ContactsAndVerification) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library bob(other.path());
    ASSERT_TRUE(alice.addContact(bob.contactCode()));
    const QString key = role(alice.contacts(), 0, "key");
    EXPECT_EQ(role(alice.contacts(), 0, "verification"), QStringLiteral("none"));
    alice.setVerification(key, QStringLiteral("in-person"));
    EXPECT_EQ(role(alice.contacts(), 0, "verification"), QStringLiteral("in-person"));
    EXPECT_EQ(role(alice.contacts(), 0, "fingerprint"), bob.fingerprint());
    alice.removeContact(key);
    EXPECT_EQ(alice.contacts()->rowCount(), 0);
}

TEST_F(LibraryTest, FailuresAreReportedNotThrown) {
    Library library(dir_.path());
    QSignalSpy errors(&library, &Library::errorOccurred);
    EXPECT_TRUE(library.joinInvite(QStringLiteral("not a code")).isEmpty());
    EXPECT_FALSE(library.addContact(library.contactCode()));
    library.setVerification(QStringLiteral("zz"), QStringLiteral("call"));
    library.renameBoard(QStringLiteral("nope"), QStringLiteral("x"));
    EXPECT_EQ(errors.count(), 4);
}

TEST_F(LibraryTest, RemovingTheCurrentProjectSelectsAnother) {
    Library library(dir_.path());
    const QString first = library.createProject(QStringLiteral("First"));
    const QString second = library.createProject(QStringLiteral("Second"));
    library.removeProject(second);
    EXPECT_EQ(library.currentProjectId(), first);
    library.removeProject(first);
    EXPECT_TRUE(library.currentProjectId().isEmpty());
    EXPECT_EQ(library.projects()->rowCount(), 0); // deletions are tombstones, hidden
}

} // namespace
} // namespace peerchalk
