pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// A page board's columns and their subheadings; clicking one moves the board there.
ListView {
    id: outline

    required property BoardCanvas canvas

    model: canvas.pageOutline
    clip: true
    spacing: 2
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: ItemDelegate {
        id: entry

        required property var modelData

        width: ListView.view.width
        leftPadding: entry.modelData.level === 1 ? 8 : 22
        highlighted: false
        onClicked: outline.canvas.showItem(entry.modelData.columnId, entry.modelData.itemId)

        contentItem: Row {
            spacing: 8

            Label {
                width: entry.availableWidth - count.width - 8
                text: entry.modelData.text.length > 0 ? entry.modelData.text
                    : entry.modelData.level === 1 ? qsTr("Untitled column") : qsTr("Subheading")
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: entry.modelData.level === 1 ? Font.DemiBold : Font.Normal
                color: entry.modelData.text.length > 0 ? Theme.ink : Theme.muted
            }

            Label {
                id: count

                text: entry.modelData.pages ?? ""
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        background: Rectangle {
            radius: 8
            color: entry.hovered ? Theme.hover : "transparent"
        }
    }
}
