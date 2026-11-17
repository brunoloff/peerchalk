#include "ui/library.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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

TEST_F(LibraryTest, ANonMemberWaitsAndSeesNoBoards) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library bob(other.path());
    const QString project = alice.createProject(QStringLiteral("P"));
    alice.createBoard(QStringLiteral("Board"));
    bob.joinInvite(alice.projectInvite(project));
    // Bob receives the project's manifests but is not a member.
    const QUuid id = QUuid::fromString(project);
    for (const SignedManifest &m : alice.workspace().manifestsForProject(id))
        bob.workspace().acceptManifest(m.payload, m.signature);
    bob.workspaceUpdated();
    EXPECT_TRUE(bob.currentProjectPending());
    EXPECT_EQ(bob.boards()->rowCount(), 0);
    alice.admitToProject(project, bob.userKey(), QStringLiteral("Bob"));
    for (const SignedManifest &m : alice.workspace().manifestsForProject(id))
        bob.workspace().acceptManifest(m.payload, m.signature);
    bob.workspaceUpdated();
    EXPECT_FALSE(bob.currentProjectPending());
    EXPECT_EQ(bob.boards()->rowCount(), 1);
    EXPECT_EQ(alice.contacts()->rowCount(), 1); // approving also remembers the person
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

Stroke dot(float x) {
    Stroke stroke(0xFF000000U, 2.0F);
    stroke.addPoint({x, 0.0F, 1.0F});
    return stroke;
}

TEST_F(LibraryTest, BoardDocumentsAreSharedSavedAndReloaded) {
    QString boardId;
    {
        Library library(dir_.path());
        library.createProject(QStringLiteral("P"));
        boardId = library.createBoard(QStringLiteral("B"));
        BoardDocument *document = library.openBoard(boardId);
        ASSERT_NE(document, nullptr);
        EXPECT_EQ(library.openBoard(boardId), document);
        document->board().addStroke(dot(1.0F));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !library.boardState(boardId).isEmpty(); }));
        document->board().addStroke(dot(2.0F));
    } // the destructor saves the second stroke
    Library reloaded(dir_.path());
    EXPECT_EQ(reloaded.openBoard(boardId)->board().strokes().size(), 2U);
}

TEST_F(LibraryTest, LoadingABoardDoesNotCountAsAnEdit) {
    QString boardId;
    {
        Library library(dir_.path());
        library.createProject(QStringLiteral("P"));
        boardId = library.createBoard(QStringLiteral("B"));
        library.openBoard(boardId)->board().addStroke(dot(1.0F));
        library.saveBoards();
    }
    Library reloaded(dir_.path());
    const QDateTime edited = reloaded.workspace().board(QUuid::fromString(boardId)).edited;
    reloaded.openBoard(boardId);
    QTest::qWait(700);
    EXPECT_EQ(reloaded.workspace().board(QUuid::fromString(boardId)).edited, edited);
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
