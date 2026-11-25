#include "library/access_log.hpp"
#include "library/user_identity.hpp"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <memory>

namespace peerchalk {
namespace {

using Action = AccessOp::Action;

struct User {
    QTemporaryDir dir;
    UserIdentity identity{dir.path()};
    [[nodiscard]] QByteArray key() const { return identity.userKey(); }
};

// Builds and replays one resource's changes signed by several users.
class AccessLogTest : public ::testing::Test {
  protected:
    void SetUp() override { add(admin_, Action::Create, {}, Role::None, QStringLiteral("Thesis")); }

    void add(const User &signer, Action action, const QByteArray &user = {}, Role role = Role::None,
             const QString &name = {}, ResourceKind kind = ResourceKind::Project,
             const QUuid &resource = {}) {
        AccessOp op;
        op.kind = kind;
        op.resource = resource.isNull() ? id_ : resource;
        op.clock = ++clock_;
        op.signer = signer.key();
        op.action = action;
        op.name = name;
        op.user = user;
        op.role = role;
        ops_.push_back(signOp(op, signer.identity.signer()));
    }
    void setRole(const User &signer, const User &user, Role role) {
        add(signer, Action::SetRole, user.key(), role, QStringLiteral("Someone"));
    }
    [[nodiscard]] AccessState state(const BoardRoleLookup &boards = {}) const {
        return replay(ops_, ResourceKind::Project, id_, admin_.key(), boards);
    }

    User admin_, owner_, member_, other_;
    QUuid id_ = QUuid::createUuid();
    quint64 clock_{0};
    std::vector<SignedOp> ops_;
};

TEST_F(AccessLogTest, CreatorIsAdmin) {
    const AccessState s = state();
    ASSERT_TRUE(s.exists);
    EXPECT_EQ(s.name, QStringLiteral("Thesis"));
    EXPECT_EQ(s.role(admin_.key()), Role::Admin);
    EXPECT_EQ(s.role(other_.key()), Role::None);
}

TEST_F(AccessLogTest, AdminManagesOwnersAndOwnersManageMembers) {
    setRole(admin_, owner_, Role::Owner);
    setRole(owner_, member_, Role::Member);
    setRole(owner_, other_, Role::Owner);   // owners cannot make owners
    setRole(member_, other_, Role::Member); // members cannot add anyone
    const AccessState s = state();
    EXPECT_EQ(s.role(owner_.key()), Role::Owner);
    EXPECT_EQ(s.role(member_.key()), Role::Member);
    EXPECT_EQ(s.role(other_.key()), Role::None);
}

TEST_F(AccessLogTest, OwnersCannotRemoveOwnersOrTheAdmin) {
    setRole(admin_, owner_, Role::Owner);
    setRole(admin_, other_, Role::Owner);
    setRole(owner_, other_, Role::None);
    setRole(owner_, admin_, Role::None);
    setRole(owner_, member_, Role::Member);
    setRole(owner_, member_, Role::None); // but they remove members
    const AccessState s = state();
    EXPECT_EQ(s.role(other_.key()), Role::Owner);
    EXPECT_EQ(s.role(admin_.key()), Role::Admin);
    EXPECT_EQ(s.role(member_.key()), Role::None);
}

TEST_F(AccessLogTest, AdminRoleCanBeHandedOver) {
    setRole(admin_, owner_, Role::Owner);
    add(admin_, Action::TransferAdmin, owner_.key(), Role::None, QStringLiteral("New admin"));
    setRole(admin_, member_, Role::Owner); // the old admin is now only an owner
    setRole(owner_, other_, Role::Owner);  // the new admin may
    const AccessState s = state();
    EXPECT_EQ(s.admin, owner_.key());
    EXPECT_EQ(s.role(admin_.key()), Role::Owner);
    EXPECT_EQ(s.role(member_.key()), Role::None);
    EXPECT_EQ(s.role(other_.key()), Role::Owner);
}

TEST_F(AccessLogTest, OnlyTheAnchoredCreatorCounts) {
    const AccessState forged = replay(ops_, ResourceKind::Project, id_, other_.key(), {});
    EXPECT_FALSE(forged.exists);
}

TEST_F(AccessLogTest, OwnersSetBoardLinksAndRemovalsAreRemembered) {
    const auto board = [&](const User &signer, Action action, const QByteArray &hash = {},
                           const QByteArray &user = {}, Role role = Role::None) {
        AccessOp op;
        op.kind = ResourceKind::Board;
        op.resource = id_;
        op.clock = ++clock_;
        op.signer = signer.key();
        op.action = action;
        op.linkHash = hash;
        op.user = user;
        op.role = role;
        op.name = action == Action::Create ? QStringLiteral("Board") : QString();
        const SignedOp signedOp = signOp(op, signer.identity.signer());
        ASSERT_TRUE(verifyOp(signedOp.payload, signedOp.signature).has_value());
        ops_.push_back(signedOp);
    };
    ops_.clear();
    const QByteArray first(32, 'a');
    board(admin_, Action::Create);
    board(admin_, Action::SetRole, {}, owner_.key(), Role::Owner);
    board(admin_, Action::SetRole, {}, member_.key(), Role::Member);
    board(member_, Action::SetLink, first); // members cannot
    const auto current = [&] { return replay(ops_, ResourceKind::Board, id_, admin_.key(), {}); };
    EXPECT_TRUE(current().linkHash.isEmpty());
    board(owner_, Action::SetLink, first);
    EXPECT_EQ(current().linkHash, first);
    board(owner_, Action::SetRole, {}, member_.key(), Role::None);
    EXPECT_TRUE(current().removed.contains(member_.key()));
    board(owner_, Action::SetRole, {}, member_.key(), Role::Member);
    EXPECT_FALSE(current().removed.contains(member_.key()));
    board(owner_, Action::SetLink);
    EXPECT_TRUE(current().linkHash.isEmpty());
}

TEST_F(AccessLogTest, RemovalsCarryTheirSnapshot) {
    AccessOp op;
    op.kind = ResourceKind::Board;
    op.resource = id_;
    op.clock = 9;
    op.signer = admin_.key();
    op.action = Action::SetRole;
    op.user = member_.key();
    op.role = Role::None;
    op.snapshot = QByteArray(32, 's');
    const SignedOp signedOp = signOp(op, admin_.identity.signer());
    const std::optional<SignedOp> verified = verifyOp(signedOp.payload, signedOp.signature);
    ASSERT_TRUE(verified.has_value());
    EXPECT_EQ(verified->op.snapshot, op.snapshot);
    op.snapshot = QByteArray("short"); // not a SHA-256
    const SignedOp malformed = signOp(op, admin_.identity.signer());
    EXPECT_FALSE(verifyOp(malformed.payload, malformed.signature).has_value());
}

TEST_F(AccessLogTest, TamperedChangesDoNotVerify) {
    QByteArray payload = ops_.front().payload;
    payload.replace("Thesis", "Thesiz");
    EXPECT_FALSE(verifyOp(payload, ops_.front().signature).has_value());
    EXPECT_TRUE(verifyOp(ops_.front().payload, ops_.front().signature).has_value());
}

TEST_F(AccessLogTest, OnlyTheAdminRenamesAndDeletes) {
    setRole(admin_, owner_, Role::Owner);
    add(owner_, Action::Rename, {}, Role::None, QStringLiteral("Mine"));
    add(owner_, Action::Delete);
    EXPECT_EQ(state().name, QStringLiteral("Thesis"));
    EXPECT_FALSE(state().deleted);
    add(admin_, Action::Delete);
    EXPECT_TRUE(state().deleted);
}

TEST_F(AccessLogTest, ConcurrentChangesFromTwoOwnersBothApply) {
    setRole(admin_, owner_, Role::Owner);
    setRole(admin_, other_, Role::Owner);
    setRole(owner_, member_, Role::Member);
    AccessOp concurrent;
    concurrent.resource = id_;
    concurrent.clock = clock_;
    concurrent.signer = other_.key();
    concurrent.action = Action::SetRole;
    concurrent.user = QByteArray(32, 'x');
    concurrent.role = Role::Member;
    ops_.push_back(signOp(concurrent, other_.identity.signer()));
    std::vector<SignedOp> reversed(ops_.rbegin(), ops_.rend());
    const AccessState a = state();
    const AccessState b = replay(reversed, ResourceKind::Project, id_, admin_.key(), {});
    EXPECT_EQ(a.roles, b.roles); // the order changes arrive in does not matter
    EXPECT_EQ(a.role(member_.key()), Role::Member);
    EXPECT_EQ(a.role(QByteArray(32, 'x')), Role::Member);
}

TEST_F(AccessLogTest, BoardsJoinProjectsOnlyByOwnersOfBoth) {
    const QUuid ownedBoard = QUuid::createUuid();
    const QUuid foreignBoard = QUuid::createUuid();
    setRole(admin_, owner_, Role::Owner);
    const BoardRoleLookup boards = [&](const QUuid &board, const QByteArray &,
                                       const QByteArray &user) {
        return board == ownedBoard && user == owner_.key() ? Role::Owner : Role::None;
    };
    const auto setBoard = [&](const User &signer, const QUuid &board, bool included) {
        AccessOp op;
        op.resource = id_;
        op.clock = ++clock_;
        op.signer = signer.key();
        op.action = Action::SetBoard;
        op.board = board;
        op.user = signer.key();
        op.included = included;
        ops_.push_back(signOp(op, signer.identity.signer()));
    };
    setBoard(owner_, ownedBoard, true);
    setBoard(owner_, foreignBoard, true); // not an owner of that board
    setBoard(admin_, foreignBoard, true); // the admin does not own it either
    EXPECT_EQ(state(boards).boards, std::vector<QUuid>{ownedBoard});
    setBoard(admin_, ownedBoard, false); // but the admin may remove any board
    EXPECT_TRUE(state(boards).boards.empty());
}

} // namespace
} // namespace peerchalk
