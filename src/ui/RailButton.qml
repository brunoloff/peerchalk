import QtQuick
import QtQuick.Controls

// A square icon button; text is its accessible name and tooltip.
AbstractButton {
    id: control

    required property string iconPath
    property bool selected: false

    implicitWidth: 44
    implicitHeight: 44
    hoverEnabled: true
    Accessible.name: text
    ToolTip.visible: hovered && text.length > 0
    ToolTip.text: text
    ToolTip.delay: 600

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.selected ? Theme.accentSoft : control.hovered && control.enabled ? Theme.hover : "transparent"
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            path: control.iconPath
            color: control.selected ? Theme.accent : Theme.ink
            opacity: control.enabled ? 1 : 0.35
        }
    }
}
