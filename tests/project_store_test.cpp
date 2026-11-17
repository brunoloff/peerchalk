#include "library/files.hpp"
#include "library/project_store.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <stdexcept>

namespace peerchalk {
namespace {

class ProjectStoreTest : public ::testing::Test {
  protected:
    QTemporaryDir dir_;
};

TEST_F(ProjectStoreTest, StartsEmptyAndPersistsProjectsAndBoards) {
    QUuid boardId;
    {
        ProjectStore store(dir_.path());
        EXPECT_TRUE(store.projects().empty());
        const ProjectInfo &project = store.createProject(QStringLiteral("  Thesis  "));
        EXPECT_EQ(project.name, QStringLiteral("Thesis"));
        EXPECT_EQ(project.capability.size(), capabilitySize);
        boardId = store.createBoard(project.id, QStringLiteral("Chapter 3")).id;
        store.saveBoardState(boardId, QByteArray("yrs-bytes"));
    }
    const ProjectStore reloaded(dir_.path());
    ASSERT_EQ(reloaded.projects().size(), 1U);
    ASSERT_EQ(reloaded.projects().front().boards.size(), 1U);
    EXPECT_EQ(reloaded.board(boardId).name, QStringLiteral("Chapter 3"));
    EXPECT_EQ(reloaded.boardState(boardId), QByteArray("yrs-bytes"));
}

TEST_F(ProjectStoreTest, ProjectListIsReadableOnlyByTheOwner) {
    ProjectStore store(dir_.path());
    store.createProject(QStringLiteral("Secret"));
    const QFile::Permissions permissions =
        QFile::permissions(QDir(dir_.path()).filePath(QStringLiteral("projects.json")));
    EXPECT_FALSE(permissions & (QFile::ReadGroup | QFile::ReadOther));
}

TEST_F(ProjectStoreTest, JoiningUsesTheInvite) {
    ProjectStore alice(dir_.path() + QStringLiteral("/alice"));
    const ProjectInfo &original = alice.createProject(QStringLiteral("Shared"));
    ProjectStore bob(dir_.path() + QStringLiteral("/bob"));
    const ProjectInfo &joined = bob.joinProject({original.id, original.name, original.capability});
    EXPECT_EQ(joined.id, original.id);
    EXPECT_EQ(joined.capability, original.capability);
    EXPECT_THROW(bob.joinProject({original.id, original.name, original.capability}),
                 std::invalid_argument);
}

TEST_F(ProjectStoreTest, RemovingAProjectDeletesItsBoardFiles) {
    ProjectStore store(dir_.path());
    const QUuid projectId = store.createProject(QStringLiteral("Temp")).id;
    const QUuid boardId = store.createBoard(projectId, QStringLiteral("B")).id;
    store.saveBoardState(boardId, QByteArray("x"));
    store.removeProject(projectId);
    EXPECT_TRUE(store.projects().empty());
    EXPECT_TRUE(QDir(dir_.path() + QStringLiteral("/boards")).isEmpty());
}

TEST_F(ProjectStoreTest, RenamesAndRemovesBoards) {
    ProjectStore store(dir_.path());
    const QUuid projectId = store.createProject(QStringLiteral("P")).id;
    const QUuid boardId = store.createBoard(projectId, QStringLiteral("Old")).id;
    store.renameBoard(boardId, QStringLiteral("New"));
    EXPECT_EQ(store.board(boardId).name, QStringLiteral("New"));
    store.removeBoard(boardId);
    EXPECT_THROW(static_cast<void>(store.board(boardId)), std::invalid_argument);
}

TEST_F(ProjectStoreTest, RejectsBadArguments) {
    ProjectStore store(dir_.path());
    EXPECT_THROW(store.createProject(QStringLiteral("   ")), std::invalid_argument);
    EXPECT_THROW(store.createBoard(QUuid::createUuid(), QStringLiteral("B")),
                 std::invalid_argument);
    EXPECT_THROW(store.saveBoardState(QUuid::createUuid(), QByteArray()), std::invalid_argument);
}

TEST_F(ProjectStoreTest, CorruptFilesFailLoudly) {
    writeFile(QDir(dir_.path()).filePath(QStringLiteral("projects.json")), "{nope", false);
    EXPECT_THROW(ProjectStore store(dir_.path()), std::runtime_error);
    writeFile(QDir(dir_.path()).filePath(QStringLiteral("projects.json")),
              R"({"version": 99, "projects": []})", false);
    EXPECT_THROW(ProjectStore store(dir_.path()), std::runtime_error);
}

} // namespace
} // namespace peerchalk
