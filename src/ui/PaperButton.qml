import QtQuick
import QtQuick.Controls

// A text button in the Paper style: filled ("primary"), dark ("strong"), outlined, or with
// no outline until hovered ("flat").
Button {
    id: control

    property bool primary: false
    property bool strong: false
    property bool destructive: false

    implicitHeight: 40
    leftPadding: 14
    rightPadding: 14
    font.weight: control.primary || control.strong ? Font.DemiBold : Font.Medium

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.primary || control.strong ? "white" : control.destructive ? Theme.danger : Theme.ink
        opacity: control.enabled ? 1 : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.primary ? Theme.accent
             : control.strong ? Theme.ink
             : control.hovered ? Theme.hover
             : control.flat ? "transparent" : Theme.surface
        border.width: control.primary || control.strong || control.flat ? 0 : 1
        border.color: Theme.border
        opacity: control.down ? 0.85 : 1
    }
}
