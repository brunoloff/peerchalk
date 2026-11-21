pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// A page board's columns and their subheadings; clicking one moves the board there.
ListView {
    id: outline

    required property BoardCanvas canvas

    model: canvas.pageOutline
    clip: true
    topMargin: 2
    spacing: 2
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: ItemDelegate {
        id: entry

        required property var modelData
        readonly property bool current: modelData.level === 1
            && modelData.columnId === outline.canvas.currentPage.columnId

        width: ListView.view.width
        implicitHeight: modelData.level === 1 ? 30 : 24
        topPadding: 0
        bottomPadding: 0
        leftPadding: entry.modelData.level === 1 ? 8 : 20
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
                color: entry.current ? Theme.accentInk
                     : entry.modelData.level === 1 && entry.modelData.text.length > 0 ? Theme.ink
                     : Theme.muted
            }

            Label {
                id: count

                text: entry.modelData.pages ?? ""
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        background: Rectangle {
            radius: 7
            color: entry.current ? Theme.accentSoft : entry.hovered ? Theme.hover : "transparent"
        }
    }
}
