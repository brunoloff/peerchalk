#include "ui/app_updater.hpp"

#include "ui/desktop_launcher.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#ifndef __EMSCRIPTEN__
#include <QProcess>
#endif
#include <QSysInfo>
#include <QVersionNumber>

#include <utility>

namespace peerchalk {
namespace {

constexpr int transferTimeoutMs = 60000;

} // namespace

QUrl latestReleaseApi() {
    return QUrl(QStringLiteral("https://api.github.com/repos/brunoloff/peerchalk/releases/latest"));
}

std::optional<LatestRelease> parseLatestRelease(const QByteArray &json) {
    const QJsonObject release = QJsonDocument::fromJson(json).object();
    QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (!tag.startsWith(QLatin1Char('v')) || QVersionNumber::fromString(tag.mid(1)).isNull())
        return std::nullopt;
    LatestRelease result{
        tag.mid(1), QUrl(release.value(QStringLiteral("html_url")).toString()), {}};
    for (const QJsonValue &asset : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject file = asset.toObject();
        result.assets.insert(file.value(QStringLiteral("name")).toString(),
                             QUrl(file.value(QStringLiteral("browser_download_url")).toString()));
    }
    return result;
}

bool isNewerVersion(const QString &candidate, const QString &current) {
    return QVersionNumber::fromString(candidate) > QVersionNumber::fromString(current);
}

QString appImageFileName(const QString &version) {
    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QStringLiteral("arm64"))
        arch = QStringLiteral("aarch64");
    return QStringLiteral("peerchalk-%1-linux-%2.AppImage").arg(version, arch);
}

QByteArray listedChecksum(const QByteArray &sums, const QString &file) {
    for (const QByteArray &line : sums.split('\n')) {
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() == 2 && QString::fromUtf8(parts[1]).remove(QLatin1Char('*')) == file)
            return parts[0].toLower();
    }
    return {};
}

AppUpdater::AppUpdater(QString appImage, QUrl releaseApi, QObject *parent)
    : QObject(parent), appImage_(std::move(appImage)), releaseApi_(std::move(releaseApi)),
      network_(new QNetworkAccessManager(this)) {
    network_->setTransferTimeout(transferTimeoutMs);
}

AppUpdater::~AppUpdater() = default;

AppUpdater *AppUpdater::create(QQmlEngine *engine, QJSEngine *) {
    auto *shared = engine->property("appUpdater").value<AppUpdater *>();
    if (shared == nullptr)
        return new AppUpdater(qEnvironmentVariable("APPIMAGE"));
    QJSEngine::setObjectOwnership(shared, QJSEngine::CppOwnership);
    return shared;
}

QNetworkReply *AppUpdater::get(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PeerChalk/%1").arg(DesktopLauncher::appVersion()));
    return network_->get(request);
}

void AppUpdater::setStatus(Status status, const QString &problem) {
    status_ = status;
    problem_ = problem;
    emit changed();
}

void AppUpdater::fail(const QString &problem) {
    download_.reset();
    setStatus(Status::Failed, problem);
}

void AppUpdater::check() {
    if (status_ == Status::Checking || status_ == Status::Downloading)
        return;
    setStatus(Status::Checking);
    startCheck(false);
}

void AppUpdater::checkQuietly() {
    if (status_ == Status::Idle && !checking_)
        startCheck(true);
}

// An asked-for check takes over a quiet one already under way and reports its answer.
void AppUpdater::startCheck(bool quiet) {
    quiet_ = quiet;
    if (checking_)
        return;
    checking_ = true;
    QNetworkReply *reply = get(releaseApi_);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finishCheck(reply); });
}

void AppUpdater::finishCheck(QNetworkReply *reply) {
    reply->deleteLater();
    checking_ = false;
    if (reply->error() != QNetworkReply::NoError)
        return settleCheck(Status::Failed,
                           tr("Could not reach GitHub: %1").arg(reply->errorString()));
    const std::optional<LatestRelease> release = parseLatestRelease(reply->readAll());
    if (!release)
        return settleCheck(Status::Failed, tr("GitHub's answer did not name a release."));
    latest_ = *release;
    settleCheck(isNewerVersion(latest_.version, DesktopLauncher::appVersion()) ? Status::Available
                                                                               : Status::UpToDate);
}

// A quiet check that found nothing new leaves no trace beyond a line in the log.
void AppUpdater::settleCheck(Status status, const QString &problem) {
    if (quiet_ && status != Status::Available) {
        if (!problem.isEmpty())
            qInfo("PeerChalk skipped its update check: %s", qPrintable(problem));
        return setStatus(Status::Idle);
    }
    setStatus(status, problem);
}

void AppUpdater::install() {
    const QString name = appImageFileName(latest_.version);
    if (!canInstall() || status_ != Status::Available)
        return;
    if (!latest_.assets.contains(name) || !latest_.assets.contains(QStringLiteral("SHA256SUMS")))
        return fail(tr("The release has no %1 for this computer.").arg(name));
    progress_ = 0;
    setStatus(Status::Downloading);
    QNetworkReply *reply = get(latest_.assets.value(QStringLiteral("SHA256SUMS")));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
        reply->deleteLater();
        const QByteArray checksum = listedChecksum(reply->readAll(), name);
        if (reply->error() != QNetworkReply::NoError || checksum.isEmpty())
            return fail(tr("Could not read the release's checksums."));
        downloadAppImage(checksum);
    });
}

// Writes the new AppImage beside the running one under the release's file name, so that
// a failed download never leaves a broken file in its place.
void AppUpdater::downloadAppImage(const QByteArray &checksum) {
    installedPath_ = QFileInfo(appImage_).dir().filePath(appImageFileName(latest_.version));
    download_ = std::make_unique<QSaveFile>(installedPath_);
    if (!download_->open(QIODevice::WriteOnly))
        return fail(tr("Could not write to %1: %2")
                        .arg(QFileInfo(installedPath_).absolutePath(), download_->errorString()));
    hash_.reset();
    QNetworkReply *reply = get(latest_.assets.value(appImageFileName(latest_.version)));
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        const QByteArray part = reply->readAll();
        hash_.addData(part);
        if (download_ && download_->write(part) != part.size())
            reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        progress_ = total > 0 ? static_cast<double>(got) / static_cast<double>(total) : 0;
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, checksum] { finishAppImage(reply, checksum); });
}

void AppUpdater::finishAppImage(QNetworkReply *reply, const QByteArray &checksum) {
    reply->deleteLater();
    if (!download_)
        return;
    if (reply->error() != QNetworkReply::NoError)
        return fail(tr("The download stopped: %1").arg(reply->errorString()));
    if (hash_.result().toHex() != checksum)
        return fail(tr("The download does not match the release's checksum."));
    if (!download_->commit())
        return fail(tr("Could not save the new version: %1").arg(download_->errorString()));
    download_.reset();
    QFile::setPermissions(installedPath_, QFile::permissions(installedPath_) | QFile::ExeOwner |
                                              QFile::ExeGroup | QFile::ExeOther);
    if (QFileInfo(installedPath_) != QFileInfo(appImage_) && !QFile::remove(appImage_))
        qWarning("PeerChalk could not remove the old AppImage %s", qPrintable(appImage_));
    setStatus(Status::Installed);
    emit installed(installedPath_);
}

void AppUpdater::restart() {
#ifdef __EMSCRIPTEN__ // browsers start no processes, and have no AppImage to install
    const bool started = false;
#else
    const bool started =
        status_ == Status::Installed && QProcess::startDetached(installedPath_, {});
#endif
    if (!started)
        return fail(tr("Could not start the new version; start PeerChalk again by hand."));
    QCoreApplication::quit();
}

} // namespace peerchalk
