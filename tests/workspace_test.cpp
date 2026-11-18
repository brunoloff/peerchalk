#include "library/files.hpp"
#include "library/user_identity.hpp"
#include "library/workspace.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace peerchalk {
namespace {

// One user's installation: identity plus workspace in a temporary folder.
struct Person {
    explicit Person(QString name) : name(std::move(name)) {}
    QString name;
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
    std::unique_ptr<Workspace> workspace =
        std::make_unique<Workspace>(dir.path(), identity.userKey(), identity.signer());
    [[nodiscard]] QByteArray key() const { return identity.userKey(); }
    void reload() {
        workspace = std::make_unique<Workspace>(dir.path(), identity.userKey(), identity.signer());
    }
};

std::vector<std::pair<QByteArray, QByteArray>> wire(const std::vector<SignedOp> &ops) {
    std::vector<std::pair<QByteArray, QByteArray>> result;
    for (const SignedOp &op : ops)
        result.emplace_back(op.payload, op.signature);
    return result;
}

// Delivers what `from` offers for a project to `to`, as a connection would.
void sendProject(const Person &from, Person &to, const QUuid &project) {
    to.workspace->acceptOps(wire(from.workspace->opsForProject(project)));
}

TEST(Workspace, CreatorIsAdminAndBoardsJoinTheProject) {
    Person alice("Alice");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project, alice.name);
    EXPECT_EQ(alice.workspace->projectRole(project, alice.key()), Role::Admin);
    EXPECT_EQ(alice.workspace->boardRole(board, alice.key()), Role::Admin);
    EXPECT_EQ(alice.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_TRUE(alice.workspace->sharedBoards().empty());
}

TEST(Workspace, ProjectMembersSeeAndMayOpenItsBoards) {
    Person alice("Alice");
    Person bob("Bob");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project, alice.name);
    bob.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, bob, project);
    EXPECT_FALSE(bob.workspace->mayAccessProject(bob.key(), project)); // not let in yet
    EXPECT_FALSE(bob.workspace->hasBoard(board));
    alice.workspace->setProjectRole(project, bob.key(), bob.name, Role::Member);
    sendProject(alice, bob, project);
    EXPECT_TRUE(bob.workspace->mayAccessProject(bob.key(), project));
    EXPECT_EQ(bob.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_TRUE(bob.workspace->mayAccessBoard(bob.key(), board));
    EXPECT_EQ(bob.workspace->boardRole(board, bob.key()), Role::None); // member via the project
}

TEST(Workspace, OwnersAddMembersButNotOwners) {
    Person alice("Alice");
    Person bob("Bob");
    Person carol("Carol");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    alice.workspace->setProjectRole(project, bob.key(), bob.name, Role::Owner);
    bob.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, bob, project);
    bob.workspace->setProjectRole(project, carol.key(), carol.name, Role::Member);
    EXPECT_THROW(bob.workspace->setProjectRole(project, carol.key(), carol.name, Role::Owner),
                 std::invalid_argument);
    EXPECT_THROW(bob.workspace->renameProject(project, QStringLiteral("Mine")),
                 std::invalid_argument);
    sendProject(bob, alice, project); // Bob's change reaches Alice
    EXPECT_EQ(alice.workspace->projectRole(project, carol.key()), Role::Member);
    EXPECT_NO_THROW(static_cast<void>(bob.workspace->inviteToProject(project)));
}

TEST(Workspace, AdminCanHandOverTheProject) {
    Person alice("Alice");
    Person bob("Bob");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    alice.workspace->setProjectRole(project, bob.key(), bob.name, Role::Member);
    alice.workspace->transferProjectAdmin(project, bob.key(), bob.name);
    EXPECT_EQ(alice.workspace->projectRole(project, alice.key()), Role::Owner);
    EXPECT_EQ(alice.workspace->projectRole(project, bob.key()), Role::Admin);
    EXPECT_THROW(alice.workspace->renameProject(project, QStringLiteral("x")),
                 std::invalid_argument);
}

TEST(Workspace, ABoardCanBelongToSeveralProjects) {
    Person alice("Alice");
    const QUuid first = alice.workspace->createProject(QStringLiteral("A"), alice.name);
    const QUuid second = alice.workspace->createProject(QStringLiteral("B"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Shared"), first, alice.name);
    alice.workspace->setBoardInProject(board, second, true);
    EXPECT_EQ(alice.workspace->boardsInProject(first), std::vector<QUuid>{board});
    EXPECT_EQ(alice.workspace->boardsInProject(second), std::vector<QUuid>{board});
    alice.workspace->setBoardInProject(board, first, false);
    EXPECT_TRUE(alice.workspace->boardsInProject(first).empty());
}

TEST(Workspace, BoardInviteGrantsJustThatBoardOnceApproved) {
    Person alice("Alice");
    Person carol("Carol");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), project, alice.name);
    carol.workspace->join(alice.workspace->inviteToBoard(board));
    EXPECT_EQ(carol.workspace->sharedBoards(), std::vector<QUuid>{board}); // waiting
    alice.workspace->setBoardRole(board, carol.key(), carol.name, Role::Member);
    carol.workspace->acceptOps(wire(alice.workspace->opsForBoard(board)));
    EXPECT_TRUE(carol.workspace->mayAccessBoard(carol.key(), board));
    EXPECT_FALSE(alice.workspace->mayAccessProject(carol.key(), project));
}

TEST(Workspace, ForgedChangesHaveNoEffect) {
    Person alice("Alice");
    Person eve("Eve");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Thesis"), alice.name);
    eve.workspace->join(alice.workspace->inviteToProject(project));
    sendProject(alice, eve, project);
    // Eve signs a change making herself an owner; it verifies but she has no right to it.
    AccessOp op;
    op.resource = project;
    op.clock = 99;
    op.signer = eve.key();
    op.action = AccessOp::Action::SetRole;
    op.user = eve.key();
    op.role = Role::Owner;
    const SignedOp forged = signOp(op, eve.identity.signer());
    alice.workspace->acceptOps({{forged.payload, forged.signature}});
    EXPECT_EQ(alice.workspace->projectRole(project, eve.key()), Role::None);
}

TEST(Workspace, AdminDeletionReachesMembers) {
    Person alice("Alice");
    Person bob("Bob");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Temp"), alice.name);
    alice.workspace->setProjectRole(project, bob.key(), bob.name, Role::Member);
    bob.workspace->join(alice.workspace->inviteToProject(project));
    alice.workspace->removeProject(project);
    sendProject(alice, bob, project);
    EXPECT_TRUE(bob.workspace->project(project).deleted());
    EXPECT_FALSE(bob.workspace->mayAccessProject(bob.key(), project));
}

TEST(Workspace, PersistsAcrossReloads) {
    Person alice("Alice");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Keep"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Board"), project, alice.name);
    alice.workspace->saveBoardState(board, QByteArray("yrs"));
    alice.reload();
    EXPECT_EQ(alice.workspace->boardsInProject(project), std::vector<QUuid>{board});
    EXPECT_EQ(alice.workspace->boardState(board), QByteArray("yrs"));
    const QFile::Permissions permissions =
        QFile::permissions(QDir(alice.dir.path()).filePath(QStringLiteral("workspace.json")));
    EXPECT_FALSE(permissions & (QFile::ReadGroup | QFile::ReadOther));
}

TEST(Workspace, MigratesTheFirstFileFormat) {
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
    EXPECT_EQ(workspace.boardRole(board, identity.userKey()), Role::Admin);
}

TEST(Workspace, MigratesSignedManifestsAndKeepsMembers) {
    QTemporaryDir dir;
    const UserIdentity identity(dir.path());
    const QUuid project = QUuid::createUuid();
    const QByteArray member(32, 'm');
    const QJsonObject manifest{
        {"name", "Managed"},
        {"members",
         QJsonArray{QJsonObject{{"key", QString::fromLatin1(member.toBase64())}, {"name", "Mo"}}}}};
    const QJsonObject root{
        {"projects",
         QJsonArray{QJsonObject{
             {"id", project.toString(QUuid::WithoutBraces)},
             {"capability", QString::fromLatin1(QByteArray(32, 'c').toBase64())},
             {"admin", QString::fromLatin1(identity.userKey().toBase64())},
             {"inviteName", "Managed"},
             {"manifest",
              QJsonObject{{"payload",
                           QString::fromLatin1(QJsonDocument(manifest).toJson().toBase64())}}}}}},
        {"boards", QJsonArray{}}};
    writeJsonObject(QDir(dir.path()).filePath(QStringLiteral("workspace.json")), root, 2, true);
    const Workspace workspace(dir.path(), identity.userKey(), identity.signer());
    EXPECT_EQ(workspace.projectRole(project, member), Role::Member);
    EXPECT_EQ(workspace.projectRole(project, identity.userKey()), Role::Admin);
}

TEST(Workspace, YourOwnDevicesShareEverything) {
    Person alice("Alice");
    const QUuid project = alice.workspace->createProject(QStringLiteral("Mine"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Board"), project, alice.name);
    Person laptop("Alice");
    laptop.identity.adopt(alice.identity.transferCode(QStringLiteral("Alice")));
    laptop.reload();
    EXPECT_TRUE(laptop.workspace->mergePersonal(alice.workspace->exportPersonal()));
    EXPECT_FALSE(laptop.workspace->mergePersonal(alice.workspace->exportPersonal()));
    EXPECT_TRUE(laptop.workspace->mayAccessBoard(laptop.key(), board));
    EXPECT_EQ(laptop.workspace->inviteToProject(project).capability,
              alice.workspace->project(project).capability);
}

TEST(Workspace, RejectsMisuse) {
    Person alice("Alice");
    EXPECT_THROW(alice.workspace->createProject(QStringLiteral(" "), alice.name),
                 std::invalid_argument);
    EXPECT_THROW(alice.workspace->createBoard(QStringLiteral("B"), QUuid::createUuid(), alice.name),
                 std::invalid_argument);
    const QUuid project = alice.workspace->createProject(QStringLiteral("P"), alice.name);
    EXPECT_THROW(alice.workspace->join(alice.workspace->inviteToProject(project)),
                 std::invalid_argument);
    EXPECT_THROW(alice.workspace->setProjectRole(project, alice.key(), alice.name, Role::None),
                 std::invalid_argument);
}

TEST(Workspace, BoardLinksAdmitOnlyWithTheCurrentTokenAndNotRemovedPeople) {
    Person alice("Alice"), bob("Bob"), carol("Carol");
    const QUuid board = alice.workspace->createBoard(QStringLiteral("Sketch"), {}, alice.name);
    EXPECT_FALSE(alice.workspace->currentBoardLink(board).has_value());
    const BoardLink first = alice.workspace->createBoardLink(board);
    EXPECT_EQ(alice.workspace->currentBoardLink(board)->token, first.token);
    EXPECT_THROW(bob.workspace->createBoardLink(board), std::invalid_argument);
    bob.workspace->joinByLink(first);
    bob.workspace->acceptOps(wire(alice.workspace->opsForBoard(board)));
    EXPECT_FALSE(bob.workspace->isBoardOwner(bob.key(), board));
    EXPECT_FALSE(alice.workspace->admitByLink(board, bob.key(), bob.name, QByteArray(32, 'x')));
    EXPECT_TRUE(alice.workspace->admitByLink(board, bob.key(), bob.name, first.token));
    EXPECT_EQ(alice.workspace->boardRole(board, bob.key()), Role::Member);
    alice.workspace->setBoardRole(board, bob.key(), bob.name, Role::None);
    EXPECT_FALSE(alice.workspace->admitByLink(board, bob.key(), bob.name, first.token));
    const BoardLink second = alice.workspace->createBoardLink(board);
    EXPECT_FALSE(alice.workspace->admitByLink(board, carol.key(), carol.name, first.token));
    EXPECT_TRUE(alice.workspace->admitByLink(board, carol.key(), carol.name, second.token));
    alice.workspace->disableBoardLink(board);
    EXPECT_FALSE(alice.workspace->currentBoardLink(board).has_value());
    alice.reload();
    EXPECT_FALSE(alice.workspace->boardHasLink(board));
}

TEST(Workspace, ProjectOwnersOwnItsBoards) {
    Person alice("Alice"), bob("Bob");
    const QUuid project = alice.workspace->createProject(QStringLiteral("P"), alice.name);
    const QUuid board = alice.workspace->createBoard(QStringLiteral("B"), project, alice.name);
    EXPECT_FALSE(alice.workspace->isBoardOwner(bob.key(), board));
    alice.workspace->setProjectRole(project, bob.key(), bob.name, Role::Owner);
    EXPECT_TRUE(alice.workspace->isBoardOwner(bob.key(), board));
}

} // namespace
} // namespace peerchalk
