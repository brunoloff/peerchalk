pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// What can be done to one column of a page board, from the column ruler.
PaperMenu {
    id: menu

    required property BoardCanvas canvas
    property var column: ({}) // an entry of canvas.pageColumns
    property string previousId // the column to the left, "" for the first
    property string nextId // the column to the right, "" for the last
    property string afterNextId // the one after that, "" if none

    signal renameRequested(string columnId, string title)
    signal deleteRequested(string columnId, string title)
    // Asks for a PDF to add as a column before the column `before` ("" for the end).
    signal importRequested(string before)
    signal exportRequested(string columnId, string title)

    readonly property string columnId: column.columnId ?? ""
    readonly property var backgrounds: [{value: "blank", text: qsTr("Blank")},
        {value: "ruled", text: qsTr("Ruled")}, {value: "squared", text: qsTr("Squared")},
        {value: "dotted", text: qsTr("Dotted")}]

    function setPaper(paper: string, landscape: bool, allPages: bool) {
        canvas.setColumnPaper(columnId, paper, landscape, allPages);
    }

    function backgroundName(value: string): string {
        return backgrounds.find(entry => entry.value === value)?.text ?? "";
    }

    width: 240

    PaperMenuItem {
        text: qsTr("Rename heading…")
        onTriggered: menu.renameRequested(menu.columnId, menu.column.title ?? "")
    }

    PaperMenu {
        title: qsTr("Paper size")
        value: (menu.column.paper ?? "").length > 0 ? menu.column.paper : qsTr("Custom")

        Repeater {
            model: ["A4", "Letter", "A5", "Legal"]

            PaperMenuItem {
                required property string modelData

                text: modelData
                checkable: true
                checked: menu.column.paper === modelData
                onTriggered: menu.setPaper(modelData, menu.column.landscape ?? false, false)
            }
        }

        PaperMenuSeparator {}

        PaperMenuItem {
            text: qsTr("Landscape")
            checkable: true
            checked: menu.column.landscape ?? false
            enabled: (menu.column.paper ?? "").length > 0
            onTriggered: menu.setPaper(menu.column.paper, checked, false)
        }

        PaperMenuItem {
            text: qsTr("Apply to all pages")
            enabled: (menu.column.paper ?? "").length > 0
            onTriggered: menu.setPaper(menu.column.paper, menu.column.landscape, true)
        }
    }

    PaperMenu {
        title: qsTr("Background")
        value: menu.backgroundName(menu.column.background ?? "")

        Repeater {
            model: menu.backgrounds

            PaperMenuItem {
                required property var modelData

                text: modelData.text
                checkable: true
                checked: menu.column.background === modelData.value
                onTriggered: menu.canvas.setColumnBackground(menu.columnId, modelData.value)
            }
        }
    }

    PaperMenuSeparator {}

    PaperMenuItem {
        text: qsTr("Import a PDF here…")
        visible: Library.pdfSupported
        onTriggered: menu.importRequested(menu.nextId)
    }

    PaperMenuItem {
        text: qsTr("Add column to the left")
        onTriggered: menu.canvas.addColumn(menu.columnId)
    }

    PaperMenuItem {
        text: qsTr("Add column to the right")
        onTriggered: menu.canvas.addColumn(menu.nextId)
    }

    PaperMenuItem {
        text: qsTr("Move left")
        visible: menu.previousId.length > 0
        onTriggered: menu.canvas.moveColumn(menu.columnId, menu.previousId)
    }

    PaperMenuItem {
        text: qsTr("Move right")
        visible: menu.nextId.length > 0
        onTriggered: menu.canvas.moveColumn(menu.columnId, menu.afterNextId)
    }

    PaperMenuSeparator {}

    PaperMenuItem {
        text: qsTr("Export column as PDF…")
        onTriggered: menu.exportRequested(menu.columnId, menu.column.title ?? "")
    }

    PaperMenuItem {
        text: qsTr("Delete column")
        destructive: true
        onTriggered: menu.deleteRequested(menu.columnId, menu.column.title ?? "")
    }
}
