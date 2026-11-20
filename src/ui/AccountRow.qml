import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One account on the account screen: click to open it; its menu backs it up or removes it.
Panel {
    id: row

    required property var account

    signal openRequested()
    signal backupRequested()
    signal removeRequested()

    implicitHeight: 68
    color: openArea.containsMouse ? Theme.background : Theme.surface

    MouseArea {
        id: openArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Open account %1").arg(row.account.name)
        onClicked: row.openRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        spacing: 12

        Avatar {
            name: row.account.name
            size: 40
            color: Theme.personColors[row.account.colorIndex]
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: row.account.name
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Label {
                Layout.fillWidth: true
                text: row.account.fingerprint
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        ToolButton {
            text: "⋯"
            Accessible.name: qsTr("More for %1").arg(row.account.name)
            onClicked: menu.popup()

            AccountMenu {
                id: menu

                onBackupRequested: row.backupRequested()
                onRemoveRequested: row.removeRequested()
            }
        }
    }
}
