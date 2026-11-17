import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PeerChalk

ApplicationWindow {
    id: window

    // The side panel is docked (beside the board) unless auto-hide is on or the window
    // is narrow; then it slides over the board when revealed by hovering the right edge
    // or tapping the edge tab, and hides again after the pointer leaves it.
    property bool inspectorAutoHide: false
    readonly property bool inspectorDocked: !inspectorAutoHide && width >= 720
    property bool inspectorRevealed: false
    property bool inspectorRevealedByTap: false
    readonly property bool inspectorShown: inspectorDocked || inspectorRevealed

    function revealInspector(byTap: bool) {
        inspectorRevealed = true;
        inspectorRevealedByTap = byTap;
    }

    width: 1280
    height: 800
    visible: true
    title: qsTr("PeerChalk")
    color: Theme.background
    font.family: Theme.bodyFont

    Item {
        id: boardArea

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: window.inspectorDocked ? inspector.left : parent.right
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
            objectName: "floatingNavigator"
            canvas: canvas
            visible: !window.inspectorShown
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.gap
        }

        Panel {
            visible: !window.inspectorShown
            anchors.right: parent.right
            anchors.rightMargin: -Theme.panelRadius
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: showButton.implicitWidth + Theme.panelRadius + 4
            implicitHeight: showButton.implicitHeight + 8

            RailButton {
                id: showButton

                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Show tool options")
                iconPath: Icons.chevronLeft
                onClicked: window.revealInspector(true)
            }
        }
    }

    Item {
        width: 8
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: !window.inspectorShown

        HoverHandler {
            id: edgeHover

            onHoveredChanged: if (hovered) window.revealInspector(false)
        }
    }

    Inspector {
        id: inspector

        objectName: "inspector"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        x: window.inspectorShown ? parent.width - width : parent.width
        visible: x < parent.width
        canvas: canvas
        autoHide: window.inspectorAutoHide
        floating: !window.inspectorDocked
        onAutoHideToggled: enabled => {
            window.inspectorAutoHide = enabled;
            window.revealInspector(false);
        }
        onCloseRequested: window.inspectorRevealed = false

        Behavior on x {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }

        HoverHandler {
            id: inspectorHover

            onHoveredChanged: if (hovered) window.inspectorRevealedByTap = false
        }
    }

    Timer {
        interval: 600
        running: window.inspectorRevealed && !window.inspectorDocked && !window.inspectorRevealedByTap
            && !inspectorHover.hovered && !edgeHover.hovered
        onTriggered: window.inspectorRevealed = false
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
