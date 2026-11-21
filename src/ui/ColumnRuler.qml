pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A strip pinned over a page board naming its columns, lined up with them as the board
// moves, so that the headings stay in sight far down a column. The column in the middle of
// the view is blue. Each has a menu; + between them and "+ Column" at the end add columns.
Rectangle {
    id: ruler

    required property BoardCanvas canvas
    readonly property var columns: canvas.pageColumns
    readonly property real offsetX: width / 2 - canvas.viewCenter.x * canvas.zoom
    readonly property bool editable: !canvas.readOnly
    // The column under the middle of the view, or the nearest.
    readonly property int current: {
        const x = canvas.viewCenter.x;
        let best = -1;
        let distance = Infinity;
        for (let i = 0; i < columns.length; ++i) {
            const c = columns[i];
            const d = x < c.left ? c.left - x : x > c.right ? x - c.right : 0;
            if (d < distance) {
                distance = d;
                best = i;
            }
        }
        return best;
    }

    signal renameRequested(string columnId, string title)
    signal deleteRequested(string columnId, string title)
    signal importRequested(string before)
    signal exportRequested(string columnId, string title)

    implicitHeight: 40
    color: Qt.rgba(0.984, 0.98, 0.969, 0.96)
    clip: true

    function columnAt(index: int): var {
        return index >= 0 && index < columns.length ? columns[index] : null;
    }

    function addColumn(before: string, button: Item) {
        if (!Library.pdfSupported) {
            canvas.addColumn(before);
            return;
        }
        addMenu.before = before;
        addMenu.popup(button, 0, button.height + 4);
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Repeater {
        model: ruler.columns.length

        Rectangle {
            id: chip

            required property int index
            readonly property var column: ruler.columns[index]
            readonly property bool current: index === ruler.current

            x: column.left * ruler.canvas.zoom + ruler.offsetX
            y: 5
            width: (column.right - column.left) * ruler.canvas.zoom
            height: 30
            visible: width >= 40
            radius: 8
            color: current ? Theme.accentSoft : Theme.surface
            border.color: current ? Theme.accent : Theme.border

            TapHandler {
                onTapped: ruler.canvas.showColumn(chip.column.columnId)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 3
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: chip.column.title.length > 0 ? chip.column.title : qsTr("Untitled column")
                    elide: Text.ElideRight
                    font.family: Theme.headingFont
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: chip.current ? Theme.accentInk
                         : chip.column.title.length > 0 ? Theme.ink : Theme.muted
                }

                Label {
                    text: chip.column.pages
                    font.pixelSize: 11
                    color: Theme.muted
                    visible: chip.width > 110
                }

                RailButton {
                    implicitWidth: 24
                    implicitHeight: 24
                    visible: ruler.editable && chip.width > 70
                    selected: columnMenu.visible && columnMenu.columnId === chip.column.columnId
                    text: qsTr("%1: column menu").arg(chip.column.title)
                    iconPath: Icons.more
                    onClicked: {
                        columnMenu.column = chip.column;
                        columnMenu.previousId = ruler.columnAt(chip.index - 1)?.columnId ?? "";
                        columnMenu.nextId = ruler.columnAt(chip.index + 1)?.columnId ?? "";
                        columnMenu.afterNextId = ruler.columnAt(chip.index + 2)?.columnId ?? "";
                        columnMenu.popup(this, 0, height + 6);
                    }
                }
            }
        }
    }

    // Round + in the gaps between neighbouring columns.
    Repeater {
        model: ruler.editable ? Math.max(0, ruler.columns.length - 1) : 0

        AbstractButton {
            id: plus

            required property int index
            readonly property var before: ruler.columns[index]
            readonly property var after: ruler.columns[index + 1]

            x: (before.right + after.left) / 2 * ruler.canvas.zoom + ruler.offsetX - width / 2
            y: 8
            width: 24
            height: 24
            visible: (after.left - before.right) * ruler.canvas.zoom >= 26
            hoverEnabled: true
            Accessible.name: qsTr("Add a column here")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            ToolTip.delay: 600
            onClicked: ruler.addColumn(after.columnId, this)

            contentItem: Icon {
                path: Icons.plus
                color: plus.hovered ? Theme.accent : Theme.muted
                lineWidth: 2.4
            }
            padding: 5

            background: Rectangle {
                radius: width / 2
                color: Theme.surface
                border.color: plus.hovered ? Theme.accent : "#D6D2C9"
            }
        }
    }

    // "+ Column" after the last one.
    AbstractButton {
        id: addAtEnd

        readonly property var last: ruler.columnAt(ruler.columns.length - 1)

        visible: ruler.editable && last !== null
        x: last !== null ? (last.right + 16 / ruler.canvas.zoom) * ruler.canvas.zoom + ruler.offsetX : 0
        y: 5
        height: 30
        // Fixed from the label: the default sizing loops through the dashed frame.
        implicitWidth: endLabel.implicitWidth + leftPadding + rightPadding
        hoverEnabled: true
        leftPadding: 10
        rightPadding: 10
        onClicked: ruler.addColumn("", this)

        contentItem: Text {
            id: endLabel

            text: qsTr("+ Column")
            font.pixelSize: 12
            font.weight: Font.Medium
            color: addAtEnd.hovered ? Theme.accent : Theme.muted
            verticalAlignment: Text.AlignVCenter
        }

        background: DashedFrame {
            radius: 8
            lineWidth: 1
            color: addAtEnd.hovered ? Theme.accent : "#B9B4AA"
            fill: addAtEnd.hovered ? Qt.rgba(1, 1, 1, 0.7) : "transparent"
        }
    }

    PaperMenu {
        id: addMenu

        property string before

        PaperMenuItem {
            text: qsTr("Blank pages")
            onTriggered: ruler.canvas.addColumn(addMenu.before)
        }

        PaperMenuItem {
            text: qsTr("Import a PDF…")
            onTriggered: ruler.importRequested(addMenu.before)
        }
    }

    ColumnMenu {
        id: columnMenu

        objectName: "columnMenu"
        canvas: ruler.canvas
        onRenameRequested: (columnId, title) => ruler.renameRequested(columnId, title)
        onDeleteRequested: (columnId, title) => ruler.deleteRequested(columnId, title)
        onImportRequested: before => ruler.importRequested(before)
        onExportRequested: (columnId, title) => ruler.exportRequested(columnId, title)
    }
}
