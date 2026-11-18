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
    EXPECT_TRUE(bob.projectInvite(project).isEmpty()); // only owners invite
}

// Delivers the signed access changes `from` holds to `to`, as a connection would.
void deliver(Library &from, Library &to) {
    std::vector<std::pair<QByteArray, QByteArray>> ops;
    for (const SignedOp &op : from.workspace().allOps())
        ops.emplace_back(op.payload, op.signature);
    to.workspace().acceptOps(ops);
    to.workspaceUpdated();
}

TEST_F(LibraryTest, ANonMemberWaitsAndSeesNoBoards) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library bob(other.path());
    const QString project = alice.createProject(QStringLiteral("P"));
    alice.createBoard(QStringLiteral("Board"));
    bob.joinInvite(alice.projectInvite(project));
    deliver(alice, bob);
    EXPECT_TRUE(bob.currentProjectPending());
    EXPECT_EQ(bob.boards()->rowCount(), 0);
    alice.admitToProject(project, bob.userKey(), QStringLiteral("Bob"));
    deliver(alice, bob);
    EXPECT_FALSE(bob.currentProjectPending());
    EXPECT_EQ(bob.boards()->rowCount(), 1);
    EXPECT_EQ(bob.projectRole(project), QStringLiteral("member"));
    EXPECT_EQ(alice.contacts()->rowCount(), 1); // approving also remembers the person
}

TEST_F(LibraryTest, ABoardInviteIsApprovedByTheBoardOwner) {
    QTemporaryDir other;
    Library alice(dir_.path());
    Library carol(other.path());
    alice.createProject(QStringLiteral("P"));
    const QString board = alice.createBoard(QStringLiteral("Board"));
    carol.joinInvite(alice.boardInvite(board));
    deliver(alice, carol);
    EXPECT_EQ(carol.boardRole(board), QStringLiteral("none"));
    alice.admitToBoard(board, carol.userKey(), QStringLiteral("Carol"));
    deliver(alice, carol);
    EXPECT_EQ(carol.boardRole(board), QStringLiteral("member"));
    EXPECT_EQ(carol.sharedBoardCount(), 1);
}

TEST_F(LibraryTest, TheAdminSetsRolesAndOwnersAddMembers) {
    QTemporaryDir second;
    QTemporaryDir third;
    Library alice(dir_.path());
    Library bob(second.path());
    Library carol(third.path());
    const QString project = alice.createProject(QStringLiteral("P"));
    ASSERT_TRUE(alice.addContact(bob.contactCode()));
    alice.setProjectRole(project, bob.userKey(), QStringLiteral("owner"));
    const QVariantList people = alice.projectPeople(project);
    ASSERT_EQ(people.size(), 2);
    EXPECT_EQ(people[0].toMap()["role"].toString(), QStringLiteral("admin"));
    EXPECT_EQ(people[1].toMap()["role"].toString(), QStringLiteral("owner"));
    bob.joinInvite(alice.projectInvite(project));
    deliver(alice, bob);
    EXPECT_FALSE(bob.projectInvite(project).isEmpty()); // owners invite
    bob.admitToProject(project, carol.userKey(), QStringLiteral("Carol"));
    deliver(bob, alice);
    EXPECT_EQ(alice.projectPeople(project).size(), 3);
    alice.transferProjectAdmin(project, bob.userKey());
    EXPECT_EQ(alice.projectRole(project), QStringLiteral("owner"));
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

TEST_F(LibraryTest, CustomColoursAreRememberedAndAnnounced) {
    Library library(dir_.path());
    QSignalSpy changed(&library, &Library::customColorsChanged);
    QSignalSpy errors(&library, &Library::errorOccurred);
    library.addCustomColor(QStringLiteral("#123456"));
    EXPECT_EQ(library.customColors(), QStringList{QStringLiteral("#123456")});
    library.addCustomColor(QStringLiteral("not a colour"));
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(errors.count(), 1);
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
