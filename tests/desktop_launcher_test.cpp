#include "ui/desktop_launcher.hpp"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

QString read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

class DesktopLauncherTest : public ::testing::Test {
  protected:
    [[nodiscard]] QString appImage(const QString &name) const {
        return QDir(home_.path()).filePath(name);
    }
    QTemporaryDir home_;
};

TEST_F(DesktopLauncherTest, IsUnavailableOutsideAnAppImage) {
    DesktopLauncher launcher({}, home_.path());
    EXPECT_FALSE(launcher.available());
    EXPECT_FALSE(launcher.install());
    EXPECT_FALSE(QFile::exists(launcher.desktopFilePath()));
}

TEST_F(DesktopLauncherTest, InstallsALauncherThatStartsThisAppImage) {
    DesktopLauncher launcher(appImage(QStringLiteral("my apps/peerchalk.AppImage")), home_.path());
    EXPECT_EQ(launcher.state(), DesktopLauncher::State::Missing);
    QSignalSpy changed(&launcher, &DesktopLauncher::stateChanged);
    ASSERT_TRUE(launcher.install());
    EXPECT_EQ(changed.size(), 1);
    EXPECT_EQ(launcher.state(), DesktopLauncher::State::Current);
    const QString entry = read(launcher.desktopFilePath());
    EXPECT_TRUE(entry.contains(QStringLiteral("Exec=\"%1\" %u").arg(launcher.appImage())));
    EXPECT_TRUE(entry.contains(QStringLiteral("Icon=io.github.brunoloff.PeerChalk")));
    EXPECT_TRUE(QFile::exists(launcher.iconPath()));
}

TEST_F(DesktopLauncherTest, ALauncherForAnotherAppImageIsOutdatedUntilMoved) {
    DesktopLauncher old(appImage(QStringLiteral("peerchalk-0.1.AppImage")), home_.path());
    ASSERT_TRUE(old.install());
    DesktopLauncher current(appImage(QStringLiteral("peerchalk-0.2.AppImage")), home_.path());
    EXPECT_EQ(current.state(), DesktopLauncher::State::Outdated);
    old.moveTo(current.appImage()); // an update moves the existing launcher along
    EXPECT_EQ(current.state(), DesktopLauncher::State::Current);
}

TEST_F(DesktopLauncherTest, MovingWithoutALauncherCreatesNone) {
    DesktopLauncher launcher(appImage(QStringLiteral("a.AppImage")), home_.path());
    launcher.moveTo(appImage(QStringLiteral("b.AppImage")));
    EXPECT_EQ(launcher.state(), DesktopLauncher::State::Missing);
    EXPECT_EQ(launcher.appImage(), appImage(QStringLiteral("b.AppImage")));
}

TEST_F(DesktopLauncherTest, QuotesSpecialCharactersInTheExecLine) {
    DesktopLauncher launcher(appImage(QStringLiteral("100% \"x\".AppImage")), home_.path());
    ASSERT_TRUE(launcher.install());
    const QString entry = read(launcher.desktopFilePath());
    EXPECT_TRUE(entry.contains(QStringLiteral("100%% \\\"x\\\".AppImage\" %u")));
    EXPECT_EQ(launcher.state(), DesktopLauncher::State::Current);
}

} // namespace
} // namespace peerchalk
