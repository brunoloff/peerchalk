#include "repository_fixture.hpp"

#include <gtest/gtest.h>

namespace peerchalk::repository {
namespace {

using testing::Device;
using testing::documentOf;
using testing::draw;
using testing::hasWarning;
using testing::line;
using testing::strokeCount;

class WriterTest : public ::testing::Test {
  protected:
    void SetUp() override {
        NewBoard created =
            createBoard(QStringLiteral("Seminar"), QStringLiteral("Ada"), admin_.author);
        capability_ = created.capability;
        testing::apply(files_, created.files);
        add(member_, Role::Member);
        add(owner_, Role::Owner);
    }
    void add(const Device &device, Role role) {
        testing::apply(files_, setRole(loadBoard(files_, {}), admin_.author, capability_,
                                       device.user(), QStringLiteral("someone"), role));
    }
    [[nodiscard]] Compaction compactAs(const BoardFiles &files, const Device &device) const {
        const LoadedBoard loaded = loadBoard(files, {});
        return compact(*documentOf(loaded), loaded, device.author, {});
    }

    Device admin_;
    Device member_;
    Device owner_;
    QByteArray capability_;
    BoardFiles files_;
};

TEST_F(WriterTest, ChangeFilesHoldOnlyWhatSnapshotsLack) {
    draw(files_, member_, line(0, 10));
    testing::apply(files_, compactAs(files_, owner_).files);
    EXPECT_TRUE(changesCovered(loadBoard(files_, {}), member_.key()));
    draw(files_, member_, line(20, 30));
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_FALSE(changesCovered(loaded, member_.key()));
    EXPECT_EQ(strokeCount(loaded), 2U);
    EXPECT_EQ(loaded.snapshots.size(), 1U);
}

// Erasing adds nothing to a Yrs state vector: a change file holding only erasures must
// still count as not covered, or tidying it away would bring the strokes back.
TEST_F(WriterTest, ChangeFilesWithOnlyErasuresAreNotCovered) {
    draw(files_, member_, line(0, 10));
    testing::apply(files_, compactAs(files_, owner_).files);
    LoadedBoard loaded = loadBoard(files_, {});
    ASSERT_TRUE(changesCovered(loaded, member_.key()));
    const std::unique_ptr<Board> document = documentOf(loaded);
    ASSERT_EQ(document->eraseNear({5.0F, 0.0F}, 2.0F), 1U);
    files_[changeFilePath(member_.key())] =
        changeFile(*document, loaded, member_.author, {document->clientId()});
    loaded = loadBoard(files_, {});
    EXPECT_EQ(strokeCount(loaded), 0U);
    EXPECT_FALSE(changesCovered(loaded, member_.key()));
}

// Owners, not only the board's creator, hand its secret to the people they add.
TEST_F(WriterTest, OwnersHandTheSecretToThoseTheyAdd) {
    const Device late;
    testing::apply(files_, setRole(loadBoard(files_, {}), owner_.author, capability_, late.user(),
                                   QStringLiteral("late"), Role::Member));
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_TRUE(loaded.warnings.empty());
    EXPECT_EQ(openSecret(loaded, late.identity), capability_);
}

TEST_F(WriterTest, ConcurrentCompactionsMergeWithoutLoss) {
    draw(files_, member_, line(0, 10));
    testing::apply(files_, compactAs(files_, owner_).files); // a first snapshot
    draw(files_, admin_, line(20, 30));
    draw(files_, member_, line(40, 50));
    // Two clones compact the same state, then each draws once more, then their branches merge.
    BoardFiles first = files_;
    BoardFiles second = files_;
    testing::apply(first, compactAs(first, owner_).files);
    draw(first, owner_, line(60, 70));
    testing::apply(second, compactAs(second, admin_).files);
    draw(second, admin_, line(80, 90));
    BoardFiles merged = first;
    for (const auto &[path, bytes] : second)
        if (!files_.contains(path) || files_.at(path) != bytes)
            merged[path] = bytes; // changed on the second side
    for (const auto &[path, bytes] : files_)
        if (!second.contains(path))
            merged.erase(path); // deleted on the second side
    const LoadedBoard loaded = loadBoard(merged, {});
    EXPECT_TRUE(loaded.warnings.empty());
    EXPECT_EQ(loaded.snapshots.size(), 2U);
    EXPECT_EQ(strokeCount(loaded), 5U);
}

TEST_F(WriterTest, MembersCannotAppointOrRemove) {
    const LoadedBoard loaded = loadBoard(files_, {});
    const Device newcomer;
    EXPECT_THROW(
        (void)setRole(loaded, member_.author, capability_, newcomer.user(), {}, Role::Member),
        std::invalid_argument);
    EXPECT_THROW((void)removeMember(*documentOf(loaded), loaded, member_.author, owner_.user(), {},
                                    testing::freshSecret()),
                 std::invalid_argument);
    EXPECT_THROW(
        (void)setRole(loaded, owner_.author, capability_, newcomer.user(), {}, Role::Owner),
        std::invalid_argument); // owners add members only
}

TEST_F(WriterTest, RemovalKeepsEarlierWorkAndIgnoresLaterFiles) {
    draw(files_, member_, line(0, 10));
    const LoadedBoard before = loadBoard(files_, {});
    const QByteArray fresh = testing::freshSecret();
    testing::apply(files_, removeMember(*documentOf(before), before, owner_.author, member_.user(),
                                        {}, fresh));
    draw(files_, member_, line(20, 30)); // written after the removal: ignored
    const LoadedBoard loaded = loadBoard(files_, {});
    EXPECT_EQ(loaded.access.role(member_.user()), Role::None);
    EXPECT_EQ(strokeCount(loaded), 1U);
    EXPECT_TRUE(hasWarning(loaded, changeFilePath(member_.key())));
    EXPECT_EQ(loaded.latestEpoch(), 1);
    EXPECT_EQ(openSecret(loaded, owner_.identity), fresh);
    EXPECT_EQ(openSecret(loaded, admin_.identity), fresh);
    EXPECT_EQ(openSecret(loaded, member_.identity), capability_); // only the old one
}

TEST_F(WriterTest, RefusesEmptyNames) {
    EXPECT_THROW((void)createBoard(QStringLiteral("  "), {}, admin_.author), std::invalid_argument);
}

} // namespace
} // namespace peerchalk::repository
