pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the connection map's lines mean.
Flow {
    id: legend

    spacing: 18

    Repeater {
        model: [{ color: Theme.accent, dashed: false, text: qsTr("Direct") },
                { color: Theme.relay, dashed: true, text: qsTr("Through a relay") },
                { color: Theme.lineFaint, dashed: true, text: qsTr("Unreachable") }]

        RowLayout {
            id: entry

            required property var modelData

            spacing: 6

            Row {
                spacing: 3

                Repeater {
                    model: entry.modelData.dashed ? 3 : 1

                    Rectangle {
                        width: entry.modelData.dashed ? 4 : 18
                        height: 2.5
                        color: entry.modelData.color
                    }
                }
            }

            Label {
                text: entry.modelData.text
                font.pixelSize: 12
                color: Theme.muted
            }
        }
    }
}
