#include "library/files.hpp"
#include "library/user_identity.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace peerchalk {
namespace {

// One user's installation: identity plus workspace in a temporary folder.
struct Person {
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
    std::unique_ptr<Workspace> workspace =
        std::make_unique<Workspace>(dir.path(), identity.userKey(), identity.signer());
    [[nodiscard]] QByteArray key() const { return identity.userKey(); }
    void reload() {
        workspace = std::make_unique<Workspace>(dir.path(), identity.userKey(), identity.signer());
    }
};

// Delivers every manifest `from` would offer for a project to `to`.
void sendProject(const Person &from, Person &to, const QUuid &projectId) {
    for (const SignedManifest &m : from.workspace->manifestsForProject(projectId))
        to.workspace->acceptManifest(m.payload, m.signature);
}

TEST(Workspace, CreatorIsAdminAndMayAccess) {
    Person alice;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project);
    EXPECT_TRUE(alice.workspace->mayAccessProject(alice.key(), project));
    EXPECT_TRUE(alice.workspace->mayAccessBoard(alice.key(), board));
    EXPECT_EQ(alice.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_TRUE(alice.workspace->sharedBoards().empty());
}

TEST(Workspace, InvitedProjectMemberSeesItsBoards) {
    Person alice;
    Person bob;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project);
    alice.workspace->setProjectMembers(project, {{bob.key(), QStringLiteral("Bob")}});
    bob.workspace->join(alice.workspace->inviteToProject(project));
    EXPECT_FALSE(bob.workspace->mayAccessProject(bob.key(), project)); // no manifest yet
    sendProject(alice, bob, project);
    EXPECT_TRUE(bob.workspace->mayAccessProject(bob.key(), project));
    EXPECT_EQ(bob.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_TRUE(bob.workspace->mayAccessBoard(bob.key(), board));
    EXPECT_TRUE(alice.workspace->mayAccessBoard(bob.key(), board));
}

TEST(Workspace, NonMembersGetNoAccessEvenWithTheInvite) {
    Person alice;
    Person eve;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project);
    eve.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, eve, project);
    EXPECT_FALSE(eve.workspace->mayAccessProject(eve.key(), project));
    EXPECT_FALSE(alice.workspace->mayAccessBoard(eve.key(), board));
}

TEST(Workspace, ABoardCanBelongToSeveralProjects) {
    Person alice;
    const QUuid first = alice.workspace->createProject(QStringLiteral("A"));
    const QUuid second = alice.workspace->createProject(QStringLiteral("B"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Shared"), first);
    alice.workspace->setBoardInProject(board, second, true);
    EXPECT_EQ(alice.workspace->boardsInProject(first), std::vector<QUuid>{board});
    EXPECT_EQ(alice.workspace->boardsInProject(second), std::vector<QUuid>{board});
    alice.workspace->setBoardInProject(board, first, false);
    EXPECT_TRUE(alice.workspace->boardsInProject(first).empty());
}

TEST(Workspace, BoardOnlyInviteGrantsJustThatBoard) {
    Person alice;
    Person carol;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project);
    alice.workspace->setBoardMembers(board, {{carol.key(), QStringLiteral("Carol")}});
    carol.workspace->join(alice.workspace->inviteToBoard(board));
    for (const SignedManifest &m : alice.workspace->manifestsForBoard(board))
        carol.workspace->acceptManifest(m.payload, m.signature);
    EXPECT_TRUE(carol.workspace->mayAccessBoard(carol.key(), board));
    EXPECT_EQ(carol.workspace->sharedBoards(), std::vector<QUuid>{board});
    EXPECT_FALSE(alice.workspace->mayAccessProject(carol.key(), project));
}

TEST(Workspace, OnlyTheAdminChangesManifests) {
    Person alice;
    Person bob;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"));
    alice.workspace->setProjectMembers(project, {{bob.key(), QStringLiteral("Bob")}});
    bob.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, bob, project);
    EXPECT_THROW(bob.workspace->renameProject(project, QStringLiteral("Mine")),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(bob.workspace->inviteToProject(project)), std::invalid_argument);
    // A manifest Bob signs himself is not accepted as the project's.
    const SignedManifest forged = signManifest(
        {ResourceKind::Project, project, QStringLiteral("Mine"), bob.key(), 99, {}, {}, false},
        bob.identity.signer());
    EXPECT_FALSE(alice.workspace->acceptManifest(forged.payload, forged.signature));
}

TEST(Workspace, OlderManifestsAreIgnored) {
    Person alice;
    Person bob;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Old"));
    bob.workspace->join(alice.workspace->inviteToProject(project));
    const SignedManifest old = alice.workspace->manifestsForProject(project).front();
    alice.workspace->renameProject(project, QStringLiteral("New"));
    sendProject(alice, bob, project);
    EXPECT_FALSE(bob.workspace->acceptManifest(old.payload, old.signature));
    EXPECT_EQ(bob.workspace->project(project).name(), QStringLiteral("New"));
}

TEST(Workspace, AdminDeletionReachesMembers) {
    Person alice;
    Person bob;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Temp"));
    alice.workspace->setProjectMembers(project, {{bob.key(), QStringLiteral("Bob")}});
    bob.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, bob, project);
    alice.workspace->removeProject(project);
    sendProject(alice, bob, project);
    EXPECT_TRUE(bob.workspace->project(project).deleted());
    EXPECT_FALSE(bob.workspace->mayAccessProject(bob.key(), project));
}

TEST(Workspace, PersistsAcrossReloads) {
    Person alice;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Keep"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Board"), project);
    alice.workspace->saveBoardState(board, QByteArray("yrs"));
    alice.reload();
    EXPECT_EQ(alice.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_EQ(alice.workspace->boardState(board), QByteArray("yrs"));
    const QFile::Permissions permissions =
        QFile::permissions(QDir(alice.dir.path()).filePath(QStringLiteral("workspace.json")));
    EXPECT_FALSE(permissions & (QFile::ReadGroup | QFile::ReadOther));
}

TEST(Workspace, MigratesTheOldProjectsFile) {
    QTemporaryDir dir;
    const QUuid project = QUuid::createUuid();
    const QUuid board = QUuid::createUuid();
    const QJsonObject old{
        {"projects",
         QJsonArray{QJsonObject{
             {"id", project.toString(QUuid::WithoutBraces)},
             {"name", "Old project"},
             {"capability", QString::fromLatin1(QByteArray(32, 'c').toBase64())},
             {"color", 1},
             {"boards", QJsonArray{QJsonObject{{"id", board.toString(QUuid::WithoutBraces)},
                                               {"name", "Old board"},
                                               {"edited", "2026-11-17T10:00:00.000Z"}}}}}}}};
    writeJsonObject(QDir(dir.path()).filePath(QStringLiteral("projects.json")), old, 1, true);
    const UserIdentity identity(dir.path());
    const Workspace workspace(dir.path(), identity.userKey(), identity.signer());
    EXPECT_EQ(workspace.project(project).name(), QStringLiteral("Old project"));
    EXPECT_EQ(workspace.boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_EQ(workspace.board(board).admin, identity.userKey());
}

TEST(Workspace, YourOwnDevicesShareEverything) {
    Person alice;
    const QUuid project = alice.workspace->createProject(QStringLiteral("Mine"));
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Board"), project);
    Person laptop;
    laptop.identity.adopt(alice.identity.transferCode(QStringLiteral("Alice")));
    laptop.reload();
    EXPECT_TRUE(laptop.workspace->mergePersonal(alice.workspace->exportPersonal()));
    EXPECT_FALSE(laptop.workspace->mergePersonal(alice.workspace->exportPersonal()));
    EXPECT_TRUE(laptop.workspace->mayAccessBoard(laptop.key(), board));
    EXPECT_EQ(laptop.workspace->inviteToProject(project).capability,
              alice.workspace->project(project).capability);
}

TEST(Workspace, RejectsMisuse) {
    Person alice;
    EXPECT_THROW(alice.workspace->createProject(QStringLiteral(" ")), std::invalid_argument);
    EXPECT_THROW(alice.workspace->createBoard(QStringLiteral("B"), QUuid::createUuid()),
                 std::invalid_argument);
    const QUuid project = alice.workspace->createProject(QStringLiteral("P"));
    EXPECT_THROW(alice.workspace->join(alice.workspace->inviteToProject(project)),
                 std::invalid_argument);
}

} // namespace
} // namespace peerchalk
