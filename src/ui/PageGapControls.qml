import QtQuick
import QtQuick.Layouts

// The gap before a page or subheading of a page board, or at the end of a column: hovering
// it offers a page or a subheading there; the end of a column always offers a page.
Item {
    id: gap

    required property BoardCanvas canvas
    required property bool editable
    required property string columnId
    required property string itemId // "" at the end of the column
    required property real itemX
    required property real itemY
    required property real itemWidth
    required property real itemHeight
    readonly property bool atEnd: itemId.length === 0

    signal subheadingRequested()

    visible: editable && height >= 10

    HoverHandler {
        id: hover
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 6
        visible: hover.hovered || gap.atEnd

        PagePill {
            text: gap.atEnd ? qsTr("+ Add page") : qsTr("+ Page")
            onClicked: gap.canvas.addPage(gap.columnId, gap.itemId)
        }

        PagePill {
            visible: hover.hovered
            text: qsTr("+ Subheading")
            onClicked: gap.subheadingRequested()
        }
    }
}
