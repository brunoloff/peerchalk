import QtQuick
import QtQuick.Controls

// A one-line text field in the Paper style, ringed in the accent colour while typing.
TextField {
    id: field

    implicitHeight: 42
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 15
    color: Theme.ink
    placeholderTextColor: Theme.muted
    selectByMouse: true

    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.surface
        border.color: field.activeFocus ? Theme.accent : Theme.border
        border.width: 1

        Rectangle { // the focus ring
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.color: Qt.rgba(0.184, 0.357, 0.918, 0.15)
            border.width: 3
            visible: field.activeFocus
        }
    }
}
