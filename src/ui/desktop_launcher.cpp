#include "ui/desktop_launcher.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#ifndef __EMSCRIPTEN__
#include <QProcess>
#endif
#include <QSaveFile>
#include <QStandardPaths>

#include <stdexcept>
#include <utility>

namespace peerchalk {
namespace {

const QString appId = QStringLiteral("io.github.brunoloff.PeerChalk");
const QString pathKey = QStringLiteral("X-PeerChalk-AppImage=");
const QString versionKey = QStringLiteral("X-PeerChalk-Version=");

// A path as one argument of a desktop file's Exec line: quoted, with the characters the
// specification reserves escaped.
QString quotedForExec(QString path) {
    for (const QString &special :
         {QStringLiteral("\\"), QStringLiteral("\""), QStringLiteral("`"), QStringLiteral("$")})
        path.replace(special, QStringLiteral("\\") + special);
    path.replace(QStringLiteral("%"), QStringLiteral("%%"));
    return QLatin1Char('"') + path + QLatin1Char('"');
}

// The value of a key in a desktop file's text, or "" without it.
QString valueOf(const QString &entry, const QString &key) {
    for (const QString &line : entry.split(QLatin1Char('\n')))
        if (line.startsWith(key))
            return line.mid(key.size());
    return {};
}

void writeFile(const QString &path, const QByteArray &contents) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() ||
        !file.commit())
        throw std::runtime_error(
            QStringLiteral("Could not write %1: %2").arg(path, file.errorString()).toStdString());
}

} // namespace

DesktopLauncher::DesktopLauncher(QString appImage, QString dataHome, QObject *parent)
    : QObject(parent), appImage_(std::move(appImage)), dataHome_(std::move(dataHome)) {}

DesktopLauncher *DesktopLauncher::create(QQmlEngine *, QJSEngine *) {
    return new DesktopLauncher(
        qEnvironmentVariable("APPIMAGE"),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
}

QString DesktopLauncher::appVersion() { return QStringLiteral(PEERCHALK_VERSION); }

QString DesktopLauncher::desktopFilePath() const {
    return QDir(dataHome_).filePath(QStringLiteral("applications/%1.desktop").arg(appId));
}

QString DesktopLauncher::iconPath() const {
    return QDir(dataHome_).filePath(QStringLiteral("icons/hicolor/512x512/apps/%1.png").arg(appId));
}

DesktopLauncher::State DesktopLauncher::state() const {
    QFile file(desktopFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return State::Missing;
    const QString entry = QString::fromUtf8(file.readAll());
    return valueOf(entry, pathKey) == appImage_ && valueOf(entry, versionKey) == appVersion()
               ? State::Current
               : State::Outdated;
}

QString DesktopLauncher::desktopEntry() const {
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=PeerChalk\n"
                          "GenericName=Shared whiteboard\n"
                          "Comment=Draw and write together, directly between devices\n"
                          "Exec=%1 %u\n"
                          "TryExec=%2\n"
                          "Icon=%3\n"
                          "Terminal=false\n"
                          "Categories=Graphics;\n"
                          "Keywords=whiteboard;drawing;collaboration;notes;latex;\n"
                          "StartupWMClass=PeerChalk\n"
                          "%4%2\n"
                          "%5%6\n")
        // The icon by its path: desktops that cache icon themes, such as Plasma, would not
        // notice one added to the user's theme folder.
        .arg(quotedForExec(appImage_), appImage_, iconPath(), pathKey, versionKey, appVersion());
}

void DesktopLauncher::writeIcon() const {
    QFile icon(QStringLiteral(":/PeerChalk/peerchalk-logo.png"));
    if (!icon.open(QIODevice::ReadOnly))
        throw std::runtime_error("The app's icon is missing");
    writeFile(iconPath(), icon.readAll());
}

bool DesktopLauncher::install() {
    if (!available())
        return false;
    try {
        writeIcon();
        writeFile(desktopFilePath(), desktopEntry().toUtf8());
    } catch (const std::exception &error) {
        emit errorOccurred(QString::fromStdString(error.what()));
        return false;
    }
#ifndef __EMSCRIPTEN__ // browsers start no processes, and have no AppImage
    // Desktops that cache launchers notice the change sooner; the rest read the folder.
    const QString refresh =
        QStandardPaths::findExecutable(QStringLiteral("update-desktop-database"));
    if (!refresh.isEmpty())
        QProcess::startDetached(refresh, {QFileInfo(desktopFilePath()).absolutePath()});
#endif
    emit stateChanged();
    return true;
}

void DesktopLauncher::moveTo(const QString &appImage) {
    const bool hadLauncher = state() != State::Missing;
    appImage_ = appImage;
    if (!hadLauncher || !install())
        emit stateChanged();
}

} // namespace peerchalk
