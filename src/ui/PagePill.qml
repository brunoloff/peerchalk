import QtQuick
import QtQuick.Controls

// One half of the "+ Page | + Subheading" pill over a page board's gaps.
AbstractButton {
    id: pill

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 28
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
        color: pill.hovered ? Theme.accentSoft : "transparent"
    }
}
