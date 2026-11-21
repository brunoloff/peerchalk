import QtQuick
import QtQuick.Controls

// One choice of a few, shown as a small rounded button; give the chips of one choice a
// ButtonGroup.
AbstractButton {
    id: chip

    checkable: true
    hoverEnabled: true
    implicitWidth: label.implicitWidth + 28
    implicitHeight: 34
    font.pixelSize: 13
    font.weight: chip.checked ? Font.DemiBold : Font.Medium
    Accessible.role: Accessible.RadioButton

    contentItem: Text {
        id: label

        text: chip.text
        font: chip.font
        color: chip.checked ? "#1E40AF" : Theme.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 9
        color: chip.checked ? Theme.accentSoft : chip.hovered ? Theme.hover : Theme.surface
        border.color: chip.checked ? Theme.accent : Theme.border
        border.width: chip.visualFocus ? 2 : 1
    }
}
