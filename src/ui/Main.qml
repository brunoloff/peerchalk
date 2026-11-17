import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PeerChalk

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: qsTr("PeerChalk")
    color: Theme.background
    font.family: Theme.bodyFont

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                anchors.fill: parent
                color: Theme.boardBackground
            }

            BoardCanvas {
                id: canvas

                anchors.fill: parent
                penColor: Theme.penColors[0]
                penWidth: Theme.penWidths[1]
                gridColor: Theme.gridDot
            }

            Panel {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.gap
                implicitWidth: titleColumn.implicitWidth + 28
                implicitHeight: titleColumn.implicitHeight + 16

                ColumnLayout {
                    id: titleColumn

                    anchors.centerIn: parent
                    spacing: 0

                    Label {
                        text: qsTr("Untitled board")
                        font.weight: Font.DemiBold
                        color: Theme.ink
                    }

                    Label {
                        text: qsTr("On this device only · not shared yet")
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }
            }

            ToolRail {
                canvas: canvas
                anchors.left: parent.left
                anchors.leftMargin: Theme.gap
                anchors.verticalCenter: parent.verticalCenter
            }

            Panel {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.gap
                implicitWidth: coordinates.implicitWidth + 24
                implicitHeight: coordinates.implicitHeight + 16
                radius: Theme.controlRadius

                Label {
                    id: coordinates

                    anchors.centerIn: parent
                    text: qsTr("x %1 · y %2").arg(Math.round(canvas.viewCenter.x)).arg(Math.round(canvas.viewCenter.y))
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }

            NavigatorPanel {
                canvas: canvas
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.gap
            }
        }

        Inspector {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            visible: window.width >= 720
            canvas: canvas
        }
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        onActivated: canvas.undo()
    }

    Shortcut {
        sequence: "P"
        onActivated: canvas.tool = BoardCanvas.Pen
    }

    Shortcut {
        sequence: "M"
        onActivated: canvas.tool = BoardCanvas.Highlighter
    }

    Shortcut {
        sequence: "E"
        onActivated: canvas.tool = BoardCanvas.Eraser
    }

    Shortcut {
        sequence: "H"
        onActivated: canvas.tool = BoardCanvas.Pan
    }

    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+="]
        onActivated: canvas.zoomBy(1.25)
    }

    Shortcut {
        sequences: [StandardKey.ZoomOut]
        onActivated: canvas.zoomBy(1 / 1.25)
    }

    Shortcut {
        sequence: "Ctrl+0"
        onActivated: canvas.resetView()
    }

    Shortcut {
        sequence: "Shift+1"
        onActivated: canvas.fitContent()
    }
}
