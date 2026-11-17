import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One board: the drawing canvas with its tools, opened from the home screen. The board
// loads from the library when the page opens and saves itself shortly after each change
// and when the page closes.
Item {
    id: page

    required property string boardId
    readonly property string boardName: Library.boardName(boardId)
    property bool loaded: false
    property bool dirty: false

    signal back()

    function save() {
        if (page.dirty) {
            Library.saveBoardState(page.boardId, canvas.saveState());
            page.dirty = false;
        }
    }

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

    objectName: "boardPage"
    Component.onCompleted: {
        const state = Library.boardState(page.boardId);
        if (state.byteLength > 0)
            canvas.loadState(state);
        page.loaded = true;
    }
    Component.onDestruction: page.save()

    Timer {
        id: saveTimer

        interval: 600
        onTriggered: page.save()
    }

    Item {
        id: boardArea

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: page.inspectorDocked ? inspector.left : parent.right
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
            onStrokesChanged: {
                if (page.loaded) {
                    page.dirty = true;
                    saveTimer.restart();
                }
            }
        }

        Panel {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.gap
            implicitWidth: titleColumn.implicitWidth + 20
            implicitHeight: titleColumn.implicitHeight + 12

            RowLayout {
                id: titleColumn

                anchors.centerIn: parent
                spacing: 8

                RailButton {
                    implicitWidth: 36
                    implicitHeight: 36
                    text: qsTr("Back to %1").arg(Library.currentProjectName)
                    iconPath: Icons.chevronLeft
                    onClicked: {
                        page.save();
                        page.back();
                    }
                }

                ColumnLayout {
                    spacing: 0

                    Label {
                        text: page.boardName
                        font.weight: Font.DemiBold
                        color: Theme.ink
                    }

                    Label {
                        text: qsTr("%1 · only you so far").arg(Library.currentProjectName)
                        font.pixelSize: 12
                        color: Theme.muted
                    }
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
            visible: !page.inspectorShown
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.gap
        }

        Panel {
            visible: !page.inspectorShown
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
                onClicked: page.revealInspector(true)
            }
        }
    }

    Item {
        width: 8
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: !page.inspectorShown

        HoverHandler {
            id: edgeHover

            onHoveredChanged: if (hovered) page.revealInspector(false)
        }
    }

    Inspector {
        id: inspector

        objectName: "inspector"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        x: page.inspectorShown ? parent.width - width : parent.width
        visible: x < parent.width
        canvas: canvas
        autoHide: page.inspectorAutoHide
        floating: !page.inspectorDocked
        onAutoHideToggled: enabled => {
            page.inspectorAutoHide = enabled;
            page.revealInspector(false);
        }
        onCloseRequested: page.inspectorRevealed = false

        Behavior on x {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }

        HoverHandler {
            id: inspectorHover

            onHoveredChanged: if (hovered) page.inspectorRevealedByTap = false
        }
    }

    Timer {
        interval: 600
        running: page.inspectorRevealed && !page.inspectorDocked && !page.inspectorRevealedByTap
            && !inspectorHover.hovered && !edgeHover.hovered
        onTriggered: page.inspectorRevealed = false
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
