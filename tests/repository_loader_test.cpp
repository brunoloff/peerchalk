#include "repository_fixture.hpp"

#include "repository/signed_content.hpp"

#include <gtest/gtest.h>

namespace peerchalk::repository {
namespace {

using testing::Device;
using testing::documentOf;
using testing::draw;
using testing::hasWarning;
using testing::line;
using testing::strokeCount;

// A board created by its admin, with a member and an outsider.
class LoaderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        NewBoard created =
            createBoard(QStringLiteral("Lecture 3"), QStringLiteral("Ada"), admin_.author);
        id_ = created.id;
        capability_ = created.capability;
        testing::apply(files_, created.files);
        testing::apply(files_, setRole(loadBoard(files_, {}), admin_.author, capability_,
                                       member_.user(), QStringLiteral("Ben"), Role::Member));
    }

    Device admin_;
    Device member_;
    Device outsider_;
    QUuid id_;
    QByteArray capability_;
    BoardFiles files_;
};

TEST_F(LoaderTest, LoadsTheNewBoardWithItsNameRolesAndSecret) {
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_TRUE(loaded.warnings.empty());
    EXPECT_EQ(loaded.name, QStringLiteral("Lecture 3"));
    EXPECT_EQ(loaded.access.role(admin_.user()), Role::Admin);
    EXPECT_EQ(loaded.access.role(member_.user()), Role::Member);
    EXPECT_EQ(openSecret(loaded, admin_.identity), capability_);
    EXPECT_EQ(openSecret(loaded, member_.identity), capability_);
    EXPECT_FALSE(openSecret(loaded, outsider_.identity));
}

TEST_F(LoaderTest, MembersEditsLoadAndOutsidersAreSkipped) {
    draw(files_, member_, line(0, 10));
    draw(files_, outsider_, line(20, 30));
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_EQ(strokeCount(loaded), 1U);
    EXPECT_TRUE(hasWarning(loaded, changeFilePath(outsider_.key())));
}

TEST_F(LoaderTest, SkipsMisplacedAndForeignFiles) {
    draw(files_, member_, line(0, 10));
    const QByteArray memberFile = files_.at(changeFilePath(member_.key()));
    files_[changeFilePath(admin_.key())] = memberFile; // not named after its device
    files_[QStringLiteral("snapshots/") + toHex(QByteArray(32, 'a')) + ".yrs"] = memberFile;
    files_[QStringLiteral("notes.txt")] = "hello";
    files_[QStringLiteral("blobs/") + toHex(QByteArray(32, 'b'))] = "not that hash";
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_TRUE(hasWarning(loaded, changeFilePath(admin_.key())));
    EXPECT_TRUE(hasWarning(loaded, QStringLiteral("notes.txt")));
    EXPECT_EQ(loaded.warnings.size(), 4U);
    EXPECT_EQ(strokeCount(loaded), 1U);
}

TEST_F(LoaderTest, SkipsContentForAnotherBoard) {
    const LoadedBoard loaded = loadBoard(files_, {});
    const ContentHeader header{
        QUuid::createUuid(), ContentKind::Changes, member_.author.certificate, {}, {}, {}};
    const QString path = changeFilePath(member_.key());
    files_[path] = sealContent(header, loaded.state, member_.author.device);
    EXPECT_TRUE(hasWarning(loadBoard(files_, {}), path));
}

TEST_F(LoaderTest, RequiresBoardInfo) {
    files_.erase(QStringLiteral("board.json"));
    EXPECT_THROW((void)loadBoard(files_, {}), FileError);
}

TEST_F(LoaderTest, DeletedRemovalStillCountsThroughHistory) {
    const LoadedBoard before = loadBoard(files_, {});
    const FileChanges removal = removeMember(*documentOf(before), before, admin_.author,
                                             member_.user(), {}, testing::freshSecret());
    testing::apply(files_, removal);
    std::vector<QByteArray> history;
    for (const auto &[path, bytes] : files_)
        if (path.startsWith(QStringLiteral("access/")))
            history.push_back(bytes);
    // Someone with push rights deletes the removal entry from the branch.
    BoardFiles tampered = files_;
    for (const auto &[path, bytes] : removal)
        if (path.startsWith(QStringLiteral("access/")))
            tampered.erase(path);
    EXPECT_EQ(loadBoard(tampered, {}).access.role(member_.user()), Role::Member);
    EXPECT_EQ(loadBoard(tampered, history).access.role(member_.user()), Role::None);
}

TEST_F(LoaderTest, RefusesRemovalWithoutSnapshot) {
    const LoadedBoard loaded = loadBoard(files_, {});
    AccessOp op;
    op.kind = ResourceKind::Board;
    op.resource = id_;
    op.clock = loaded.access.clock + 1;
    op.signer = admin_.user();
    op.action = AccessOp::Action::SetRole;
    op.user = member_.user();
    op.role = Role::None;
    const SignedOp entry = signOp(op, admin_.author.user);
    const QString path = QStringLiteral("access/") + accessFileName(entry);
    files_[path] = encodeAccessFile(entry);
    const LoadedBoard after = loadBoard(files_, {});
    EXPECT_TRUE(hasWarning(after, path));
    EXPECT_EQ(after.access.role(member_.user()), Role::Member);
}

TEST_F(LoaderTest, SecretsFromNonOwnersAreSkipped) {
    const BoardSecret fake{id_, 5, member_.user(), sealCapability(member_.user(), "x"),
                           member_.user()};
    const QString path = QStringLiteral("secrets/") + secretFileName(fake);
    files_[path] = encodeSecretFile(fake, member_.author.user);
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_TRUE(hasWarning(loaded, path));
    EXPECT_EQ(loaded.latestEpoch(), 0);
}

} // namespace
} // namespace peerchalk::repository
