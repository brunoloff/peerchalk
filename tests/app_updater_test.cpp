#include "ui/app_updater.hpp"

#include <QFile>
#include <QSignalSpy>
#include <QSysInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace peerchalk {
namespace {

TEST(AppUpdater, ReadsTheLatestReleaseFromGitHub) {
    const auto release = parseLatestRelease(R"({"tag_name": "v0.2.1",
        "html_url": "https://github.com/brunoloff/peerchalk/releases/tag/v0.2.1",
        "assets": [{"name": "SHA256SUMS", "browser_download_url": "https://example.com/SHA256SUMS"}]})");
    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(release->version, QStringLiteral("0.2.1"));
    EXPECT_EQ(release->page.toString(),
              QStringLiteral("https://github.com/brunoloff/peerchalk/releases/tag/v0.2.1"));
    EXPECT_EQ(release->assets.value(QStringLiteral("SHA256SUMS")).toString(),
              QStringLiteral("https://example.com/SHA256SUMS"));
}

TEST(AppUpdater, RejectsAnswersThatAreNotReleases) {
    EXPECT_FALSE(parseLatestRelease("not json").has_value());
    EXPECT_FALSE(parseLatestRelease(R"({"message": "Not Found"})").has_value());
    EXPECT_FALSE(parseLatestRelease(R"({"tag_name": "nightly"})").has_value());
}

TEST(AppUpdater, ComparesVersionsByNumber) {
    EXPECT_TRUE(isNewerVersion(QStringLiteral("0.1.10"), QStringLiteral("0.1.9")));
    EXPECT_TRUE(isNewerVersion(QStringLiteral("0.2"), QStringLiteral("0.1.1")));
    EXPECT_FALSE(isNewerVersion(QStringLiteral("0.1.1"), QStringLiteral("0.1.1")));
    EXPECT_FALSE(isNewerVersion(QStringLiteral("0.1"), QStringLiteral("0.1.1")));
}

TEST(AppUpdater, NamesTheAppImageForThisProcessor) {
    const QString name = appImageFileName(QStringLiteral("0.2"));
    EXPECT_TRUE(name.startsWith(QStringLiteral("peerchalk-0.2-linux-")));
    EXPECT_TRUE(name.endsWith(QStringLiteral(".AppImage")));
    EXPECT_FALSE(name.contains(QStringLiteral("arm64"))); // releases say aarch64
}

TEST(AppUpdater, FindsAFilesChecksumInSha256Sums) {
    const QByteArray sums = "ABC123  peerchalk-0.2-linux-x86_64.AppImage\n"
                            "def456 *peerchalk-0.2-linux-aarch64.AppImage\n";
    EXPECT_EQ(listedChecksum(sums, QStringLiteral("peerchalk-0.2-linux-x86_64.AppImage")),
              QByteArray("abc123"));
    EXPECT_EQ(listedChecksum(sums, QStringLiteral("peerchalk-0.2-linux-aarch64.AppImage")),
              QByteArray("def456"));
    EXPECT_TRUE(listedChecksum(sums, QStringLiteral("other")).isEmpty());
}

TEST(AppUpdater, InstallsOnlyFromAnAppImage) {
    EXPECT_FALSE(AppUpdater({}).canInstall());
    EXPECT_TRUE(AppUpdater(QStringLiteral("/tmp/peerchalk.AppImage")).canInstall());
    EXPECT_EQ(AppUpdater({}).status(), AppUpdater::Status::Idle); // nothing contacted yet
}

// Answers like GitHub's API, from a file the test writes.
class AppUpdaterCheckTest : public ::testing::Test {
  protected:
    QUrl answer(const QByteArray &json) {
        QFile file(dir_.filePath(QStringLiteral("latest.json")));
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(json);
        return QUrl::fromLocalFile(file.fileName());
    }
    QUrl missing() { return QUrl::fromLocalFile(dir_.filePath(QStringLiteral("missing.json"))); }

    static void waitForCheck(AppUpdater &updater) {
        QSignalSpy changed(&updater, &AppUpdater::changed);
        while (updater.status() == AppUpdater::Status::Checking || changed.isEmpty())
            ASSERT_TRUE(changed.wait(5000));
    }

    QTemporaryDir dir_;
};

TEST_F(AppUpdaterCheckTest, AQuietCheckAnnouncesANewerVersion) {
    AppUpdater updater({}, answer(R"({"tag_name": "v999.0", "html_url": "https://a.example"})"));
    updater.checkQuietly();
    EXPECT_EQ(updater.status(), AppUpdater::Status::Idle); // nothing shows while it asks
    waitForCheck(updater);
    EXPECT_EQ(updater.status(), AppUpdater::Status::Available);
    EXPECT_EQ(updater.latestVersion(), QStringLiteral("999.0"));
}

TEST_F(AppUpdaterCheckTest, AQuietCheckStaysSilentWhenUpToDateOrUnreachable) {
    AppUpdater upToDate({}, answer(R"({"tag_name": "v0.0.1"})"));
    upToDate.checkQuietly();
    waitForCheck(upToDate);
    EXPECT_EQ(upToDate.status(), AppUpdater::Status::Idle);
    AppUpdater offline({}, missing());
    offline.checkQuietly();
    waitForCheck(offline);
    EXPECT_EQ(offline.status(), AppUpdater::Status::Idle);
    EXPECT_TRUE(offline.problem().isEmpty());
}

TEST_F(AppUpdaterCheckTest, AnAskedForCheckReportsWhatItFound) {
    AppUpdater offline({}, missing());
    offline.check();
    EXPECT_EQ(offline.status(), AppUpdater::Status::Checking);
    waitForCheck(offline);
    EXPECT_EQ(offline.status(), AppUpdater::Status::Failed);
    EXPECT_FALSE(offline.problem().isEmpty());
    AppUpdater upToDate({}, answer(R"({"tag_name": "v0.0.1"})"));
    upToDate.checkQuietly();
    upToDate.check(); // asking during a quiet check takes over its answer
    waitForCheck(upToDate);
    EXPECT_EQ(upToDate.status(), AppUpdater::Status::UpToDate);
}

} // namespace
} // namespace peerchalk
