import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The gap before a page or subheading of a page board, or at the end of a column. Hovering
// a gap lights it and offers "+ Page | + Subheading" there; the end of a column has a larger
// "+ Add page" block with the paper new pages get.
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
    property string paper // "A4 · ruled", for the end of a column
    property bool alwaysShown: false // for touch screens, which cannot hover
    readonly property bool shown: hover.hovered || alwaysShown
    readonly property bool atEnd: itemId.length === 0

    signal subheadingRequested()

    visible: editable && height >= 8 && width >= 120

    HoverHandler {
        id: hover
    }

    Rectangle { // the gap lit up
        anchors.fill: parent
        visible: hover.hovered && !gap.atEnd
        radius: 3
        color: Qt.rgba(0.184, 0.357, 0.918, 0.10)
    }

    Item { // the pill, one card with two halves
        anchors.centerIn: parent
        width: pill.implicitWidth
        height: 28
        visible: gap.shown && !gap.atEnd

        SoftShadow {
            anchors.fill: parent
            radius: height / 2
            spread: 8
            strength: 0.14
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Theme.surface
            border.color: "#D6D2C9"
            clip: true

            RowLayout {
                id: pill

                anchors.fill: parent
                spacing: 0

                PagePill {
                    text: qsTr("+ Page")
                    onClicked: gap.canvas.addPage(gap.columnId, gap.itemId)
                }

                Rectangle {
                    implicitWidth: 1
                    implicitHeight: 16
                    color: Theme.border
                }

                PagePill {
                    text: qsTr("+ Subheading")
                    onClicked: gap.subheadingRequested()
                }
            }
        }
    }

    AbstractButton { // the end of the column
        id: addPage

        visible: gap.atEnd
        y: 0
        width: parent.width
        height: 64
        hoverEnabled: true
        Accessible.name: qsTr("Add a page")
        onClicked: gap.canvas.addPage(gap.columnId, "")

        background: DashedFrame {
            radius: 10
            color: addPage.hovered ? Theme.accent : "#B9B4AA"
            fill: addPage.hovered ? Qt.rgba(1, 1, 1, 0.8) : Qt.rgba(1, 1, 1, 0.55)
        }

        contentItem: ColumnLayout {
            spacing: 2

            Item {
                Layout.fillHeight: true
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("+ Add page")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: addPage.hovered ? Theme.accent : Theme.ink
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                visible: gap.paper.length > 0
                text: gap.paper
                font.pixelSize: 12
                color: Theme.muted
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
