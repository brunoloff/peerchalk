import QtQuick
import QtQuick.Controls

// A tab in the Paper style: the selected tab takes the page's background and has no
// bottom border, so it reads as part of the page beneath; the others stay on the bar.
TabButton {
    id: tab

    implicitWidth: contentItem.implicitWidth + 40
    width: implicitWidth
    implicitHeight: 48
    font.pixelSize: 15
    font.weight: tab.checked ? Font.DemiBold : Font.Normal

    contentItem: Text {
        text: tab.text
        font: tab.font
        color: tab.checked ? Theme.ink : Theme.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: tab.checked ? Theme.background : tab.hovered ? Theme.hover : "transparent"
        border.color: tab.checked ? Theme.border : "transparent"

        // Covers the bottom edge of the border so the tab opens into the page.
        Rectangle {
            visible: tab.checked
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            height: 1
            color: Theme.background
        }
    }
}
