#include "ui/app_updater.hpp"

#include <QSysInfo>

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

} // namespace
} // namespace peerchalk
