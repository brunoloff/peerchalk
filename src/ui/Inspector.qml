pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The right-hand panel with options for the current tool.
Rectangle {
    id: inspector

    required property BoardCanvas canvas
    readonly property bool drawing: canvas.tool === BoardCanvas.Pen || canvas.tool === BoardCanvas.Highlighter

    color: Theme.surface
    implicitWidth: 280

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label {
            text: {
                switch (inspector.canvas.tool) {
                case BoardCanvas.Pen: return qsTr("Pen");
                case BoardCanvas.Highlighter: return qsTr("Highlighter");
                case BoardCanvas.Eraser: return qsTr("Eraser");
                default: return qsTr("Pan");
                }
            }
            font.family: Theme.headingFont
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        SectionLabel {
            visible: inspector.drawing
            text: qsTr("Colour")
        }

        Flow {
            Layout.fillWidth: true
            visible: inspector.drawing
            spacing: 10

            Repeater {
                model: Theme.penColors

                delegate: ColorSwatch {
                    canvas: inspector.canvas
                }
            }
        }

        SectionLabel {
            visible: inspector.drawing
            text: qsTr("Width · %1 px").arg(inspector.canvas.penWidth.toFixed(0))
        }

        RowLayout {
            Layout.fillWidth: true
            visible: inspector.drawing
            spacing: 6

            Repeater {
                model: Theme.penWidths

                delegate: WidthButton {
                    canvas: inspector.canvas
                }
            }

            Slider {
                Layout.fillWidth: true
                from: 1
                to: 24
                stepSize: 1
                value: inspector.canvas.penWidth
                Accessible.name: qsTr("Pen width")
                onMoved: inspector.canvas.penWidth = value
            }
        }

        Label {
            Layout.fillWidth: true
            visible: inspector.canvas.tool === BoardCanvas.Highlighter
            wrapMode: Text.WordWrap
            text: qsTr("The highlighter draws in the pen colour, see-through and four times as wide.")
            color: Theme.muted
        }

        Label {
            Layout.fillWidth: true
            visible: !inspector.drawing
            wrapMode: Text.WordWrap
            lineHeight: 1.3
            color: Theme.muted
            text: inspector.canvas.tool === BoardCanvas.Eraser
                ? qsTr("Drag across strokes to remove them. Undo brings back everything erased in one drag.")
                : qsTr("Drag to move around the board. You can also drag with the middle mouse button, scroll to pan, or hold Ctrl and scroll to zoom.")
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
