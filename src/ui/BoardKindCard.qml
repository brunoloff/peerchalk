import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A kind of board to pick in the new-board dialog: a title and what it is for.
AbstractButton {
    id: card

    property string summary

    checkable: true
    implicitHeight: content.implicitHeight + 28
    Accessible.role: Accessible.RadioButton
    Accessible.description: card.summary

    contentItem: ColumnLayout {
        id: content

        spacing: 4

        Label {
            Layout.fillWidth: true
            text: card.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        Label {
            Layout.fillWidth: true
            text: card.summary
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            lineHeight: 1.2
            color: Theme.muted
        }
    }

    leftPadding: 14
    rightPadding: 14
    topPadding: 14
    bottomPadding: 14

    background: Rectangle {
        radius: Theme.controlRadius
        color: card.checked ? Theme.accentSoft : card.hovered ? Theme.hover : Theme.surface
        border.color: card.checked ? Theme.accent : Theme.border
        border.width: card.checked || card.visualFocus ? 2 : 1
    }
}
