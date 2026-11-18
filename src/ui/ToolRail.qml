pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// The vertical tool picker on the left of the board.
Panel {
    id: rail

    required property BoardCanvas canvas

    implicitWidth: column.implicitWidth + 12
    implicitHeight: column.implicitHeight + 12
    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Drawing tools")

    ColumnLayout {
        id: column

        anchors.centerIn: parent
        spacing: 4

        Repeater {
            model: [
                { tool: BoardCanvas.Pen, text: qsTr("Pen (P)"), icon: Icons.pen },
                { tool: BoardCanvas.Highlighter, text: qsTr("Highlighter (M)"), icon: Icons.highlighter },
                { tool: BoardCanvas.Eraser, text: qsTr("Eraser (E)"), icon: Icons.eraser },
                { tool: BoardCanvas.Pan, text: qsTr("Pan (H)"), icon: Icons.pan }
            ]

            delegate: RailButton {
                required property var modelData

                text: modelData.text
                iconPath: modelData.icon
                selected: rail.canvas.tool === modelData.tool
                onClicked: rail.canvas.tool = modelData.tool
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 4
            implicitHeight: 1
            color: Theme.border
        }

        RailButton {
            text: qsTr("Undo (Ctrl+Z)")
            iconPath: Icons.undo
            enabled: rail.canvas.canUndo && !rail.canvas.readOnly
            onClicked: rail.canvas.undo()
        }
    }
}
