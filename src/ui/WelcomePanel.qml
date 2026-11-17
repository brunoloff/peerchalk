import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shown on the home screen until there is a project.
ColumnLayout {
    id: welcome

    signal createProjectRequested()
    signal joinProjectRequested()
    signal linkDeviceRequested()

    width: Math.min(420, parent.width - 64)
    spacing: 16

    Label {
        Layout.fillWidth: true
        text: qsTr("Welcome to PeerChalk")
        font.family: Theme.headingFont
        font.pixelSize: 28
        font.weight: Font.DemiBold
        color: Theme.ink
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Boards live in projects. Start a project of your own, or join one with an invite code from a friend or colleague.")
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: Theme.muted
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 8

        PaperButton {
            text: qsTr("New project")
            primary: true
            onClicked: welcome.createProjectRequested()
        }

        PaperButton {
            text: qsTr("Join with an invite")
            onClicked: welcome.joinProjectRequested()
        }
    }

    PaperButton {
        Layout.alignment: Qt.AlignHCenter
        visible: Library.canAdoptIdentity
        flat: true
        text: qsTr("Already use PeerChalk on another device? Link this one")
        onClicked: welcome.linkDeviceRequested()
    }
}
