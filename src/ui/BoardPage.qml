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
    // Their devices. Changes only when someone comes or goes, so that the avatars stay put
    // and can be clicked while the people behind them move around.
    property var peopleDevices: []

    function refreshPeople() {
        people = Network.boardPresence(boardId);
        const devices = people.map(person => person.device);
        if (devices.join() !== peopleDevices.join())
            peopleDevices = devices;
    }

    function personOn(device: string): var {
        return people.find(person => person.device === device) ?? null;
    }

    // Moves this view to what someone else is looking at.
    function goToViewOf(device: string) {
        const person = personOn(device);
        if (person !== null)
            canvas.showArea(person.view);
    }

    function publishView() {
        Network.setPresenceView(boardId, canvas.visibleArea);
    }

    signal back()

    // Phones get a bar on top and the tools along the bottom (PhoneBoardChrome).
    readonly property bool phone: width < 600

    function leave() {
        Library.saveBoards();
        page.back();
    }

    // The side panel is docked (beside the board) unless auto-hide is on or the window
    // is narrow; then it slides over the board when revealed by hovering the right edge
    // or tapping the edge tab, and hides again after the pointer leaves it.
    property bool inspectorAutoHide: false
    readonly property bool inspectorDocked: !inspectorAutoHide && width >= 720
    property bool inspectorRevealed: false
    property bool inspectorRevealedByTap: false
    property bool networkShown: false // the connection pane, over the side panel
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
        page.publishView();
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
            color: canvas.pageBoard ? Theme.desk : Theme.boardBackground
        }

        BoardCanvas {
            id: canvas

            objectName: "boardCanvas"
            anchors.fill: parent
            penColor: Theme.penColors[0]
            penWidth: Theme.penWidths[1]
            gridColor: pageBoard ? Theme.deskDot : Theme.gridDot
            topInset: page.phone ? phoneChrome.headerBottom : pageBoard ? pageChrome.rulerBottom : 0
            showCurrentPage: inspector.showsPage && page.inspectorShown
            pageColor: Theme.surface
            document: Library.openBoard(page.boardId)
            readOnly: page.access !== "edit"
            onCursorMoved: (point, inside) => Network.setPresence(page.boardId, point, inside)
            onViewChanged: page.publishView()
            onWidthChanged: page.publishView()
            onHeightChanged: page.publishView()
        }

        PageOverlay {
            anchors.fill: parent
            canvas: canvas
            visible: canvas.pageBoard
            touch: page.phone
            onRenameSubheading: (columnId, itemId, text) =>
                                pageChrome.renameSubheading(columnId, itemId, text)
            onAddSubheadingRequested: (columnId, before) => pageChrome.addSubheading(columnId, before)
        }

        TextBoxLayer {
            anchors.fill: parent
            canvas: canvas
        }

        PageBoardChrome {
            id: pageChrome

            anchors.fill: parent
            canvas: canvas
            boardId: page.boardId
            phone: page.phone
            onReported: message => exportToast.show(message)
            visible: canvas.pageBoard
        }

        BoardTitlePanel {
            id: titlePanel

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.gap
            visible: !page.phone
            canvas: canvas
            boardName: page.boardName
            boardId: page.boardId
            others: page.people.length
            networkOpen: page.networkShown
            onNetworkToggled: page.networkShown = !page.networkShown
            onBack: page.leave()
            onReported: message => exportToast.show(message)
        }

        PhoneBoardChrome {
            id: phoneChrome

            objectName: "phoneChrome"
            anchors.fill: parent
            visible: page.phone
            canvas: canvas
            boardName: page.boardName
            live: page.access === "edit"
            onBack: page.leave()
            onReported: message => exportToast.show(message)
            onImportRequested: pageChrome.importPdf("")
            onExportColumnRequested: (columnId, title) => pageChrome.exportColumn(columnId, title)
            onToolOptionsRequested: {
                inspector.pageTab = false;
                page.revealInspector(true);
            }
        }

        Toast {
            id: exportToast
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
            visible: page.peopleDevices.length > 0

            Repeater {
                model: page.peopleDevices

                delegate: AbstractButton {
                    id: personButton

                    required property string modelData
                    readonly property var person: page.personOn(modelData)
                    readonly property string personName: person?.name ?? ""

                    implicitWidth: 34
                    implicitHeight: 34
                    hoverEnabled: true
                    Accessible.name: qsTr("Go to %1's view").arg(personName)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Go to %1's view").arg(personName)
                    ToolTip.delay: 300
                    onClicked: page.goToViewOf(modelData)

                    contentItem: Avatar {
                        name: personButton.personName
                        size: 34
                        color: Theme.personColors[(personButton.person?.colorIndex ?? 0)
                                                  % Theme.personColors.length]
                        border.color: personButton.hovered ? Theme.accent : Theme.surface
                        border.width: 2
                    }

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }

        ToolRail {
            visible: !page.phone
            canvas: canvas
            anchors.left: parent.left
            anchors.leftMargin: Theme.gap
            anchors.verticalCenter: parent.verticalCenter
        }

        Panel {
            visible: !page.phone
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.gap
            implicitWidth: coordinates.implicitWidth + 24
            implicitHeight: coordinates.implicitHeight + 16
            radius: Theme.controlRadius

            Label {
                id: coordinates

                anchors.centerIn: parent
                text: canvas.pageBoard
                    ? (canvas.currentPage.pageId === undefined ? ""
                       : qsTr("%1 · page %2 of %3")
                             .arg(canvas.currentPage.columnTitle.length > 0 ? canvas.currentPage.columnTitle : qsTr("Untitled column"))
                             .arg(canvas.currentPage.number).arg(canvas.currentPage.count))
                    : qsTr("x %1 · y %2").arg(Math.round(canvas.viewCenter.x)).arg(Math.round(canvas.viewCenter.y))
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        NavigatorPanel {
            objectName: "floatingNavigator"
            canvas: canvas
            visible: !page.inspectorShown && !page.phone
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.gap
        }

        Panel {
            visible: !page.inspectorShown && !page.phone
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
        onExportColumnRequested: (columnId, title) => pageChrome.exportColumn(columnId, title)

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

    BoardNetworkPane {
        objectName: "boardNetworkPane"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: Math.min(implicitWidth, parent.width)
        visible: page.networkShown && !page.phone
        status: titlePanel.connection
        onCloseRequested: page.networkShown = false
    }

    BoardShortcuts {
        canvas: canvas
    }
}
