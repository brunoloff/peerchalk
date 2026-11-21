pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The four page backgrounds as tiles with a small sheet each; the chosen one is blue.
GridLayout {
    id: tiles

    property string value
    property int tileHeight: 64

    signal chosen(string value)

    columns: 4
    columnSpacing: 8
    rowSpacing: 8

    Repeater {
        model: [{value: "blank", text: qsTr("Blank")}, {value: "ruled", text: qsTr("Ruled")},
                {value: "squared", text: qsTr("Squared")}, {value: "dotted", text: qsTr("Dotted")}]

        AbstractButton {
            id: tile

            required property var modelData
            readonly property bool selected: tiles.value === modelData.value

            Layout.fillWidth: true
            Layout.preferredWidth: 1 // equal shares, whatever the label
            implicitHeight: tiles.tileHeight
            hoverEnabled: true
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.text
            Accessible.checked: selected
            onClicked: tiles.chosen(modelData.value)

            contentItem: ColumnLayout {
                spacing: 5

                Item {
                    Layout.fillHeight: true
                }

                PageSwatch {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: tiles.tileHeight > 56 ? 22 : 18
                    implicitHeight: tiles.tileHeight > 56 ? 28 : 22
                    pattern: tile.modelData.value
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: tile.modelData.text
                    font.pixelSize: tiles.tileHeight > 56 ? 12 : 11
                    font.weight: tile.selected ? Font.DemiBold : Font.Medium
                    color: tile.selected ? "#1E40AF" : Theme.ink
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            background: Rectangle {
                radius: 9
                color: tile.selected ? Theme.accentSoft : tile.hovered ? Theme.hover : Theme.surface
                border.color: tile.selected ? Theme.accent : Theme.border
            }
        }
    }
}
