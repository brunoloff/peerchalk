import QtQuick
import QtQuick.Controls

// A small rounded button over a page board, such as "+ Page".
AbstractButton {
    id: pill

    implicitWidth: label.implicitWidth + 20
    implicitHeight: 26
    hoverEnabled: true
    font.pixelSize: 12
    font.weight: Font.Medium
    Accessible.name: text

    contentItem: Text {
        id: label

        text: pill.text
        font: pill.font
        color: pill.hovered ? Theme.accent : Theme.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: height / 2
        color: pill.hovered ? Theme.accentSoft : Theme.surface
        border.color: pill.hovered ? Theme.accent : Theme.border
    }
}
