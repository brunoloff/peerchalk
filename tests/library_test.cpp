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
    EXPECT_EQ(bob.joinProject(alice.projectInvite(project)), project);
    EXPECT_EQ(bob.currentProjectName(), QStringLiteral("Shared"));
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
    EXPECT_TRUE(library.joinProject(QStringLiteral("not a code")).isEmpty());
    EXPECT_FALSE(library.addContact(library.contactCode()));
    EXPECT_TRUE(library.createBoard(QStringLiteral("No project yet")).isEmpty());
    library.setVerification(QStringLiteral("zz"), QStringLiteral("call"));
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
}

} // namespace
} // namespace peerchalk
