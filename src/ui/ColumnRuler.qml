pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A strip over a page board naming its columns, lined up with them as the board moves, so
// that the headings stay in sight far down a column. Each has a menu; + adds columns.
Item {
    id: ruler

    required property BoardCanvas canvas
    readonly property var columns: canvas.pageColumns
    readonly property real offsetX: width / 2 - canvas.viewCenter.x * canvas.zoom
    readonly property bool editable: !canvas.readOnly
    // Whether the columns are wide enough on screen to be named.
    readonly property bool readable: columns.length > 0
        && columns.every(column => (column.right - column.left) * canvas.zoom >= 72)

    signal renameRequested(string columnId, string title)
    signal deleteRequested(string columnId, string title)
    signal importRequested(string before)

    implicitHeight: 40

    function columnAt(index: int): var {
        return index >= 0 && index < columns.length ? columns[index] : null;
    }

    Repeater {
        model: ruler.columns.length

        Rectangle {
            id: chip

            required property int index
            readonly property var column: ruler.columns[index]

            x: column.left * ruler.canvas.zoom + ruler.offsetX
            width: Math.max(48, (column.right - column.left) * ruler.canvas.zoom)
            height: ruler.height
            radius: Theme.controlRadius
            color: Theme.surface
            border.color: Theme.border

            TapHandler {
                onTapped: ruler.canvas.showColumn(chip.column.columnId)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 4
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: chip.column.title.length > 0 ? chip.column.title : qsTr("Untitled column")
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: chip.column.title.length > 0 ? Theme.ink : Theme.muted
                }

                Label {
                    text: chip.column.pages
                    font.pixelSize: 12
                    color: Theme.muted
                    visible: chip.width > 120
                }

                RailButton {
                    implicitWidth: 32
                    implicitHeight: 32
                    visible: ruler.editable
                    text: qsTr("%1: column menu").arg(chip.column.title)
                    iconPath: Icons.more
                    onClicked: {
                        columnMenu.column = chip.column;
                        columnMenu.previousId = ruler.columnAt(chip.index - 1)?.columnId ?? "";
                        columnMenu.nextId = ruler.columnAt(chip.index + 1)?.columnId ?? "";
                        columnMenu.afterNextId = ruler.columnAt(chip.index + 2)?.columnId ?? "";
                        columnMenu.popup(this, 0, height);
                    }
                }
            }
        }
    }

    // + between neighbouring columns and after the last.
    Repeater {
        model: ruler.editable ? ruler.columns.length : 0

        RailButton {
            id: plus

            required property int index
            readonly property var column: ruler.columns[index]
            readonly property var next: ruler.columnAt(index + 1)
            readonly property real gapCentre: next !== null ? (column.right + next.left) / 2
                                                            : column.right + 48

            x: gapCentre * ruler.canvas.zoom + ruler.offsetX - width / 2
            y: (ruler.height - height) / 2
            implicitWidth: 32
            implicitHeight: 32
            text: qsTr("Add a column here")
            iconPath: Icons.plus
            onClicked: {
                const before = next !== null ? next.columnId : "";
                if (!Library.pdfSupported) {
                    ruler.canvas.addColumn(before);
                    return;
                }
                addMenu.before = before;
                addMenu.popup(this, 0, height);
            }
        }
    }

    Menu {
        id: addMenu

        property string before

        MenuItem {
            text: qsTr("Blank pages")
            onTriggered: ruler.canvas.addColumn(addMenu.before)
        }

        MenuItem {
            text: qsTr("Import a PDF…")
            onTriggered: ruler.importRequested(addMenu.before)
        }
    }

    ColumnMenu {
        id: columnMenu

        canvas: ruler.canvas
        onRenameRequested: (columnId, title) => ruler.renameRequested(columnId, title)
        onDeleteRequested: (columnId, title) => ruler.deleteRequested(columnId, title)
        onImportRequested: before => ruler.importRequested(before)
    }
}
