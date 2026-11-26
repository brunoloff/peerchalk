pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The board's network pane: everyone with a role on the board and how this device reaches
// them, best connected first.
ColumnLayout {
    id: list

    required property var people // from Network.boardConnection

    spacing: 6

    Repeater {
        model: list.people

        RowLayout {
            id: row

            required property var modelData
            readonly property string connection: modelData.state
            readonly property bool reached: modelData.you || connection === "here" || connection === "connected"

            Layout.fillWidth: true
            spacing: 10

            Avatar {
                name: row.modelData.name
                size: 28
                color: Theme.personColors[(row.modelData.colorIndex ?? 0) % Theme.personColors.length]
                opacity: row.reached ? 1 : 0.45
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: row.modelData.name.length > 0 ? row.modelData.name : qsTr("Someone")
                    font.pixelSize: 13
                    color: Theme.ink
                }

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: ConnectionAdvice.personLine(row.modelData)
                    font.pixelSize: 12
                    color: row.connection === "here" ? Theme.workingInk : Theme.muted
                }
            }

            Label {
                text: row.modelData.role
                font.pixelSize: 11
                color: Theme.quiet
            }
        }
    }
}
