pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A choice of a few, side by side in a tray; the chosen one is a raised white tab.
Rectangle {
    id: segments

    property var choices // [{value, text}]
    property string value
    property int segmentHeight: 28
    property bool stretch: false // segments share the width equally

    signal chosen(string value)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: segmentHeight + 6
    radius: 9
    color: "#F1EFEA"
    Accessible.role: Accessible.PageTabList

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: 3
        spacing: 2

        Repeater {
            model: segments.choices

            AbstractButton {
                id: segment

                required property var modelData
                readonly property bool selected: segments.value === modelData.value

                Layout.fillWidth: segments.stretch
                implicitWidth: label.implicitWidth + 20
                implicitHeight: segments.segmentHeight
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.text
                Accessible.checked: selected
                onClicked: segments.chosen(modelData.value)

                contentItem: Text {
                    id: label

                    text: segment.modelData.text
                    font.pixelSize: 12
                    font.weight: segment.selected ? Font.DemiBold : Font.Medium
                    color: segment.selected ? Theme.ink : Theme.muted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Item {
                    SoftShadow {
                        anchors.fill: parent
                        visible: segment.selected
                        radius: 7
                        spread: 2
                        strength: 0.12
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 7
                        color: segment.selected ? Theme.surface
                             : segment.hovered ? Qt.rgba(1, 1, 1, 0.5) : "transparent"
                    }
                }
            }
        }
    }
}
