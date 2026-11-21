pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// What can be done to one column of a page board, from the column ruler.
Menu {
    id: menu

    required property BoardCanvas canvas
    property var column: ({}) // an entry of canvas.pageColumns
    property string previousId // the column to the left, "" for the first
    property string nextId // the column to the right, "" for the last
    property string afterNextId // the one after that, "" if none

    signal renameRequested(string columnId, string title)
    signal deleteRequested(string columnId, string title)

    readonly property string columnId: column.columnId ?? ""

    function setPaper(paper: string, landscape: bool, allPages: bool) {
        canvas.setColumnPaper(columnId, paper, landscape, allPages);
    }

    MenuItem {
        text: qsTr("Rename heading…")
        onTriggered: menu.renameRequested(menu.columnId, menu.column.title ?? "")
    }

    Menu {
        title: qsTr("Paper for new pages")

        Repeater {
            model: ["A4", "Letter", "A5", "Legal"]

            MenuItem {
                required property string modelData

                text: modelData
                checkable: true
                checked: menu.column.paper === modelData
                onTriggered: menu.setPaper(modelData, menu.column.landscape ?? false, false)
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Landscape")
            checkable: true
            checked: menu.column.landscape ?? false
            enabled: (menu.column.paper ?? "").length > 0
            onTriggered: menu.setPaper(menu.column.paper, checked, false)
        }

        MenuItem {
            text: qsTr("Apply to all pages in this column")
            enabled: (menu.column.paper ?? "").length > 0
            onTriggered: menu.setPaper(menu.column.paper, menu.column.landscape, true)
        }
    }

    Menu {
        title: qsTr("Background")

        Repeater {
            model: [{value: "blank", text: qsTr("Blank")}, {value: "ruled", text: qsTr("Ruled")},
                    {value: "squared", text: qsTr("Squared")},
                    {value: "dotted", text: qsTr("Dotted")}]

            MenuItem {
                required property var modelData

                text: modelData.text
                checkable: true
                checked: menu.column.background === modelData.value
                onTriggered: menu.canvas.setColumnBackground(menu.columnId, modelData.value)
            }
        }
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("Add column to the left")
        onTriggered: menu.canvas.addColumn(menu.columnId)
    }

    MenuItem {
        text: qsTr("Add column to the right")
        onTriggered: menu.canvas.addColumn(menu.nextId)
    }

    MenuItem {
        text: qsTr("Move left")
        enabled: menu.previousId.length > 0
        onTriggered: menu.canvas.moveColumn(menu.columnId, menu.previousId)
    }

    MenuItem {
        text: qsTr("Move right")
        enabled: menu.nextId.length > 0
        onTriggered: menu.canvas.moveColumn(menu.columnId, menu.afterNextId)
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("Delete column")
        onTriggered: menu.deleteRequested(menu.columnId, menu.column.title ?? "")
    }
}
