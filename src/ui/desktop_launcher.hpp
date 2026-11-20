#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace peerchalk {

// An AppImage's entry in the app launcher: a desktop file and an icon in the user's data
// folder that start the AppImage file this copy of PeerChalk runs from. Packages, Flatpaks,
// Windows, macOS and browsers set up their launchers themselves, so there it is unavailable.
// Failures emit errorOccurred with a message for the user.
class DesktopLauncher : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Launcher)
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString version READ appVersion CONSTANT)

  public:
    // Missing: no launcher; Current: it starts this AppImage at this version; Outdated: it
    // starts another file or version.
    enum class State { Missing, Current, Outdated };
    Q_ENUM(State)

    // appImage: the AppImage file, or "" when not running from one. dataHome: the folder
    // that holds applications/ and icons/, as XDG_DATA_HOME names it.
    DesktopLauncher(QString appImage, QString dataHome, QObject *parent = nullptr);
    static DesktopLauncher *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    [[nodiscard]] static QString appVersion();
    [[nodiscard]] bool available() const { return !appImage_.isEmpty(); }
    [[nodiscard]] State state() const;
    [[nodiscard]] QString desktopFilePath() const;
    [[nodiscard]] QString iconPath() const;
    [[nodiscard]] QString appImage() const { return appImage_; }

    // Writes the launcher for the AppImage and this version; returns success.
    Q_INVOKABLE bool install();
    // After an update put a new AppImage in place: refers to it, and moves an existing
    // launcher over to it.
    Q_INVOKABLE void moveTo(const QString &appImage);

  signals:
    void stateChanged();
    void errorOccurred(const QString &message);

  private:
    [[nodiscard]] QString desktopEntry() const;
    void writeIcon() const;

    QString appImage_;
    QString dataHome_;
};

} // namespace peerchalk
