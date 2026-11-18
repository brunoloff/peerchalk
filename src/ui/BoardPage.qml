pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One board: the drawing canvas with its tools, opened from the home screen. The board's
// live document comes from the library, which saves it shortly after each change; the
// page saves any remaining changes when it closes.
Item {
    id: page

    required property string boardId
    readonly property string boardName: Library.boardName(boardId)
    // "edit", "offline" (no owner connected) or "waiting" (not let in yet).
    property string access: "waiting"
    // Other devices showing this board, from Network.boardPresence.
    property var people: []

    function refreshPeople() {
        people = Network.boardPresence(boardId);
    }

    signal back()

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
        Network.setPresence(page.boardId, Qt.point(0, 0), false);
        page.refreshPeople();
        page.access = Network.boardAccess(page.boardId);
    }
    // Not on destruction: while the app closes, the views these calls update are going away
    // too, and the library saves unsaved boards itself.
    StackView.onRemoved: {
        Network.setPresence("", Qt.point(0, 0), false);
        Library.saveBoards();
    }

    Connections {
        target: Network

        function onPresenceChanged() {
            page.refreshPeople();
        }

        function onBoardAccessChanged() {
            page.access = Network.boardAccess(page.boardId);
        }
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
            document: Library.openBoard(page.boardId)
            readOnly: page.access !== "edit"
            onCursorMoved: (point, inside) => Network.setPresence(page.boardId, point, inside)
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
                        Library.saveBoards();
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
                        text: page.people.length === 0 ? qsTr("%1 · only you here").arg(Library.currentProjectName)
                            : page.people.length === 1 ? qsTr("%1 · 1 other person here").arg(Library.currentProjectName)
                            : qsTr("%1 · %2 others here").arg(Library.currentProjectName).arg(page.people.length)
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }
            }
        }

        Panel {
            objectName: "boardAccessNotice"
            visible: page.access !== "edit"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Theme.gap + 64 // below the title
            width: Math.min(accessLabel.implicitWidth + 28, parent.width - 32)
            implicitHeight: accessLabel.implicitHeight + 16

            Label {
                id: accessLabel

                anchors.fill: parent
                anchors.margins: 8
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.ink
                text: page.access === "waiting"
                    ? qsTr("Waiting for an owner of this board to come online and let you in.")
                    : qsTr("No owner is online. This is your last copy; you can draw again when one connects.")
            }
        }

        Repeater {
            model: page.people

            delegate: RemoteCursor {
                boardCanvas: canvas
            }
        }

        Row {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.gap
            spacing: -8
            visible: page.people.length > 0

            Repeater {
                model: page.people

                delegate: Avatar {
                    required property var modelData

                    name: modelData.name
                    size: 34
                    color: Theme.personColors[modelData.colorIndex % Theme.personColors.length]
                    border.color: Theme.surface
                    border.width: 2
                    ToolTip.visible: hover.hovered
                    ToolTip.text: modelData.name

                    HoverHandler {
                        id: hover
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
