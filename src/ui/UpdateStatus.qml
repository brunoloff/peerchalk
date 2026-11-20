import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What a check for a newer PeerChalk found, with the next step: installing it in an
// AppImage, or the release page elsewhere, and restarting once it is installed.
ColumnLayout {
    id: updates

    readonly property int status: Updates.status

    spacing: 8
    visible: status !== Updates.Status.Idle

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        color: updates.status === Updates.Status.Failed ? Theme.danger : Theme.muted
        text: {
            switch (updates.status) {
            case Updates.Status.Checking:
                return qsTr("Checking GitHub for a newer version…");
            case Updates.Status.UpToDate:
                return qsTr("This is the latest version, %1.").arg(Launcher.version);
            case Updates.Status.Available:
                return qsTr("PeerChalk %1 is available; this is %2.").arg(Updates.latestVersion).arg(Launcher.version);
            case Updates.Status.Downloading:
                return qsTr("Downloading PeerChalk %1…").arg(Updates.latestVersion);
            case Updates.Status.Installed:
                return qsTr("PeerChalk %1 is installed. Restart to use it.").arg(Updates.latestVersion);
            default:
                return Updates.problem;
            }
        }
    }

    ProgressBar {
        Layout.fillWidth: true
        visible: updates.status === Updates.Status.Downloading
        value: Updates.progress
    }

    Flow {
        Layout.fillWidth: true
        spacing: 10

        PaperButton {
            visible: updates.status === Updates.Status.Available && Updates.canInstall
            primary: true
            implicitHeight: 44
            text: qsTr("Update to %1").arg(Updates.latestVersion)
            onClicked: Updates.install()
        }

        PaperButton {
            visible: updates.status === Updates.Status.Available && !Updates.canInstall
            primary: true
            implicitHeight: 44
            text: qsTr("Download %1").arg(Updates.latestVersion)
            onClicked: Qt.openUrlExternally(Updates.releasePage)
        }

        PaperButton {
            visible: updates.status === Updates.Status.Installed
            primary: true
            implicitHeight: 44
            text: qsTr("Restart now")
            onClicked: Updates.restart()
        }
    }

    Connections {
        target: Updates

        function onInstalled(appImage: string) {
            Launcher.moveTo(appImage);
        }
    }
}
