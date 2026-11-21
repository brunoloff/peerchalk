import QtQuick
import QtQuick.Controls

// One choice of a few, shown as a rounded chip; give the chips of one choice a ButtonGroup.
AbstractButton {
    id: chip

    checkable: true
    implicitWidth: label.implicitWidth + 28
    implicitHeight: 32
    font.pixelSize: 13
    Accessible.role: Accessible.RadioButton

    contentItem: Text {
        id: label

        text: chip.text
        font: chip.font
        color: chip.checked ? Theme.accent : Theme.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: height / 2
        color: chip.checked ? Theme.accentSoft : chip.hovered ? Theme.hover : Theme.surface
        border.color: chip.checked ? Theme.accent : Theme.border
        border.width: chip.visualFocus ? 2 : 1
    }
}
