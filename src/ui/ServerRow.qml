import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One server in a list on the network settings page: its kind, address, where it comes from,
// whether it answers, and a button that removes it.
Rectangle {
    id: row

    // {kind, address, isDefault, browserOnly}, as Network.servers gives it.
    required property var server
    required property int index
    property var result
    readonly property string note: server.browserOnly ? qsTr("Browser version only, for now")
                                 : server.isDefault ? qsTr("Default") : qsTr("Added by you")

    signal removeRequested(int index)

    implicitHeight: content.implicitHeight + 22
    color: row.server.isDefault ? Theme.surface : "#FCFBF9"

    Rectangle {
        visible: row.index > 0
        width: parent.width
        height: 1
        color: "#EFECE6"
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 12

        ServerKindLabel {
            Layout.preferredWidth: 76
            kind: row.server.kind
            color: "#4A4741"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Label {
                Layout.fillWidth: true
                text: row.server.address
                font.family: Theme.monoFont
                font.pixelSize: 13
                elide: Text.ElideMiddle
                color: Theme.ink
            }

            Label {
                Layout.fillWidth: true
                text: row.note
                font.pixelSize: 12
                color: Theme.quiet
                elide: Text.ElideRight
            }
        }

        ServerStatus {
            result: row.result
        }

        ToolButton {
            id: removeButton

            objectName: "removeServer"
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            Accessible.name: qsTr("Remove %1").arg(row.server.address)
            ToolTip.text: Accessible.name
            ToolTip.visible: hovered
            onClicked: row.removeRequested(row.index)

            contentItem: Icon {
                path: Icons.minus
                color: Theme.quiet
            }

            background: Rectangle {
                radius: 8
                color: removeButton.hovered ? Theme.hover : "transparent"
            }
        }
    }
}
