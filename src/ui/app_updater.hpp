#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QMap>
#include <QObject>
#include <QSaveFile>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;

namespace peerchalk {

// What GitHub's API says about the latest release.
struct LatestRelease {
    QString version; // without the leading "v"
    QUrl page;
    QMap<QString, QUrl> assets; // file name to download address
};

// Reads the API's answer for the latest release; nullopt when it is not one.
[[nodiscard]] std::optional<LatestRelease> parseLatestRelease(const QByteArray &json);
[[nodiscard]] bool isNewerVersion(const QString &candidate, const QString &current);
// The release's AppImage for this processor, such as peerchalk-0.2-linux-x86_64.AppImage.
[[nodiscard]] QString appImageFileName(const QString &version);
// The checksum SHA256SUMS lists for a file, in hex; "" when it is not listed.
[[nodiscard]] QByteArray listedChecksum(const QByteArray &sums, const QString &file);
// GitHub's API address for PeerChalk's latest release.
[[nodiscard]] QUrl latestReleaseApi();

// Finds out, when asked, whether GitHub has a newer release of PeerChalk. An AppImage can
// then fetch the new AppImage, check it against the release's SHA256SUMS and put it in place
// of the running one; other installations are pointed to the release page. Nothing is
// contacted before check() or checkQuietly() is called. The app shares one updater between
// its windows, so a check or download carries on when the account screen and an account
// swap places.
class AppUpdater : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Updates)
    QML_SINGLETON
    Q_PROPERTY(Status status READ status NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QUrl releasePage READ releasePage NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)

  public:
    enum class Status { Idle, Checking, UpToDate, Available, Downloading, Installed, Failed };
    Q_ENUM(Status)

    // appImage: the AppImage file this copy runs from, or "" when it is not one.
    explicit AppUpdater(QString appImage, QUrl releaseApi = latestReleaseApi(),
                        QObject *parent = nullptr);
    ~AppUpdater() override;
    static AppUpdater *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    [[nodiscard]] Status status() const { return status_; }
    [[nodiscard]] QString latestVersion() const { return latest_.version; }
    [[nodiscard]] QUrl releasePage() const { return latest_.page; }
    [[nodiscard]] double progress() const { return progress_; }
    [[nodiscard]] QString problem() const { return problem_; }
    [[nodiscard]] bool canInstall() const { return !appImage_.isEmpty(); }

    Q_INVOKABLE void check();
    // A check at startup: the status changes only when a newer version is found.
    void checkQuietly();
    // Fetches and puts in place the latest AppImage; installed follows.
    Q_INVOKABLE void install();
    // Starts the installed AppImage and quits this one.
    Q_INVOKABLE void restart();

  signals:
    void changed();
    void installed(const QString &appImage);

  private:
    void startCheck(bool quiet);
    void finishCheck(QNetworkReply *reply);
    void settleCheck(Status status, const QString &problem = {});
    void downloadAppImage(const QByteArray &checksum);
    void finishAppImage(QNetworkReply *reply, const QByteArray &checksum);
    QNetworkReply *get(const QUrl &url);
    void setStatus(Status status, const QString &problem = {});
    void fail(const QString &problem);

    QString appImage_;
    QUrl releaseApi_;
    QString installedPath_;
    QNetworkAccessManager *network_;
    LatestRelease latest_;
    Status status_{Status::Idle};
    bool checking_{false};
    bool quiet_{false};
    double progress_{0};
    QString problem_;
    std::unique_ptr<QSaveFile> download_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
};

} // namespace peerchalk
