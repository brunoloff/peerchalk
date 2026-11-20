import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The large button that opens the account used last, with its own menu.
Button {
    id: control

    required property var account
    property string prompt: qsTr("Continue as")

    signal backupRequested()
    signal removeRequested()

    implicitHeight: 84
    leftPadding: 18
    rightPadding: 10
    Accessible.name: control.prompt + " " + control.account.name

    background: Rectangle {
        radius: Theme.panelRadius
        color: control.down ? Qt.darker(Theme.accent, 1.15) : control.hovered ? Qt.darker(Theme.accent, 1.07) : Theme.accent
    }

    contentItem: RowLayout {
        spacing: 14

        Avatar {
            name: control.account.name
            size: 50
            // The first person colour is the button's own blue, so it is darkened to show.
            color: Qt.darker(Theme.personColors[control.account.colorIndex],
                             control.account.colorIndex === 0 ? 1.5 : 1)
            border.color: "white"
            border.width: 2
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: control.prompt
                font.pixelSize: 13
                color: "white"
                opacity: 0.9
            }

            Label {
                Layout.fillWidth: true
                text: control.account.name
                elide: Text.ElideRight
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: "white"
            }
        }

        Icon {
            path: Icons.arrowRight
            color: "white"
            lineWidth: 2.2
        }

        ToolButton {
            id: moreButton

            implicitWidth: 36
            Accessible.name: qsTr("More for %1").arg(control.account.name)
            onClicked: menu.popup()

            contentItem: Icon {
                path: Icons.more
                color: "white"
                lineWidth: 3
            }

            background: Rectangle {
                radius: Theme.controlRadius
                color: "white"
                opacity: moreButton.hovered ? 0.15 : 0
            }

            AccountMenu {
                id: menu

                onBackupRequested: control.backupRequested()
                onRemoveRequested: control.removeRequested()
            }
        }
    }
}
