pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A board on a phone: a bar across the top with the board's name, its connection button and,
// on page boards, a tab per column; the page number in view; and the tools along the bottom,
// where a thumb reaches them. Settings open as a sheet from the bottom.
Item {
    id: chrome

    required property BoardCanvas canvas
    required property string boardName
    required property string boardId
    property bool networkOpen: false
    // Network.boardConnection, for the board's network pane.
    readonly property alias connection: networkButton.status
    // Embedded, Back leads to the host, if it offers that.
    readonly property string backTarget: HostLink.embedded ? HostLink.backLabel : qsTr("the project")
    readonly property real headerBottom: header.height
    readonly property var columns: canvas.pageColumns
    readonly property var current: canvas.currentPage

    signal back()
    signal networkToggled()
    signal reported(string message)
    // Asks for a PDF to add as a column at the end.
    signal importRequested()
    signal exportColumnRequested(string columnId, string title)
    // The tool's own options: colour and width.
    signal toolOptionsRequested()

    Rectangle {
        id: header

        width: parent.width
        height: content.implicitHeight
        color: Qt.rgba(0.984, 0.98, 0.969, 0.97)

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }

        ColumnLayout {
            id: content

            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 4

                RailButton {
                    visible: chrome.backTarget.length > 0
                    text: qsTr("Back to %1").arg(chrome.backTarget)
                    iconPath: Icons.chevronLeft
                    onClicked: chrome.back()
                }

                Label {
                    Layout.fillWidth: true
                    text: chrome.boardName
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    color: Theme.ink
                }

                BoardNetworkButton {
                    id: networkButton

                    objectName: "phoneNetworkButton"
                    boardId: chrome.boardId
                    paneOpen: chrome.networkOpen
                    onClicked: chrome.networkToggled()
                }

                RailButton {
                    text: qsTr("Board options")
                    iconPath: Icons.more
                    onClicked: boardMenu.popup(this, width - boardMenu.width, height)

                    PaperMenu {
                        id: boardMenu

                        PaperMenuItem {
                            text: qsTr("Export as PDF…")
                            onTriggered: boardExport.start(chrome.boardName, "", 0, 0)
                        }
                    }
                }
            }

            ListView { // the columns, as tabs
                id: tabs

                Layout.fillWidth: true
                Layout.preferredHeight: 44
                visible: chrome.canvas.pageBoard
                orientation: ListView.Horizontal
                spacing: 6
                leftMargin: 12
                rightMargin: 12
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: chrome.columns
                Accessible.role: Accessible.PageTabList
                Accessible.name: qsTr("Columns")

                delegate: AbstractButton {
                    id: tab

                    required property var modelData
                    readonly property bool selected: modelData.columnId === chrome.current.columnId

                    height: 34
                    leftPadding: 12
                    rightPadding: 12
                    Accessible.role: Accessible.PageTab
                    Accessible.checked: selected
                    onClicked: chrome.canvas.showColumn(modelData.columnId)

                    contentItem: RowLayout {
                        spacing: 6

                        Label {
                            text: tab.modelData.title.length > 0 ? tab.modelData.title : qsTr("Untitled")
                            font.family: Theme.headingFont
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: tab.selected ? Theme.accentInk : Theme.ink
                        }

                        Label {
                            text: tab.modelData.pages
                            font.pixelSize: 11
                            color: tab.selected ? Theme.accentInk : Theme.muted
                        }
                    }

                    background: Rectangle {
                        radius: 9
                        color: tab.selected ? Theme.accentSoft : Theme.surface
                        border.color: tab.selected ? Theme.accent : Theme.border
                    }
                }

                footer: Item {
                    width: addTab.width + 6
                    height: 34

                    AbstractButton {
                        id: addTab

                        x: 6
                        width: 34
                        height: 34
                        visible: !chrome.canvas.readOnly
                        Accessible.name: qsTr("Add a column")
                        onClicked: {
                            if (Library.pdfSupported)
                                addMenu.popup(this, 0, height + 4);
                            else
                                chrome.canvas.addColumn("");
                        }

                        contentItem: Icon {
                            path: Icons.plus
                            color: Theme.muted
                        }
                        padding: 8

                        background: DashedFrame {
                            radius: 9
                            lineWidth: 1
                        }
                    }
                }
            }
        }
    }

    Rectangle { // the page in view
        anchors.right: parent.right
        anchors.rightMargin: 12
        y: header.height + 12
        visible: chrome.canvas.pageBoard && chrome.current.pageId !== undefined
        implicitWidth: counter.implicitWidth + 20
        implicitHeight: counter.implicitHeight + 8
        radius: height / 2
        color: Qt.rgba(0.11, 0.106, 0.098, 0.72)

        Label {
            id: counter

            anchors.centerIn: parent
            text: qsTr("%1 / %2").arg(chrome.current.number ?? 0).arg(chrome.current.count ?? 0)
            font.pixelSize: 12
            font.weight: Font.Medium
            font.features: {"tnum": 1}
            color: "white"
        }
    }

    Item { // the tools, along the bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        anchors.bottomMargin: 16
        height: 60

        SoftShadow {
            anchors.fill: parent
            radius: 18
            spread: 16
            strength: 0.14
        }

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Theme.surface
            border.color: Theme.border
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 0

            Repeater {
                model: [
                    { tool: BoardCanvas.Pen, text: qsTr("Pen"), icon: Icons.pen },
                    { tool: BoardCanvas.Highlighter, text: qsTr("Highlighter"), icon: Icons.highlighter },
                    { tool: BoardCanvas.Text, text: qsTr("Text"), icon: Icons.text },
                    { tool: BoardCanvas.Eraser, text: qsTr("Eraser"), icon: Icons.eraser },
                    { tool: BoardCanvas.Pan, text: qsTr("Pan"), icon: Icons.pan }
                ]

                delegate: Item {
                    id: slot

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RailButton {
                        anchors.centerIn: parent
                        text: slot.modelData.text
                        iconPath: slot.modelData.icon
                        selected: chrome.canvas.tool === slot.modelData.tool
                        // A second tap on the tool in use shows its options.
                        onClicked: {
                            if (selected)
                                chrome.toolOptionsRequested();
                            chrome.canvas.tool = slot.modelData.tool;
                        }
                    }
                }
            }

            Rectangle {
                implicitWidth: 1
                implicitHeight: 28
                color: Theme.border
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RailButton {
                    anchors.centerIn: parent
                    text: qsTr("Undo")
                    iconPath: Icons.undo
                    enabled: chrome.canvas.canUndo && !chrome.canvas.readOnly
                    onClicked: chrome.canvas.undo()
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: chrome.canvas.pageBoard

                RailButton {
                    anchors.centerIn: parent
                    text: qsTr("Page and column settings")
                    iconPath: Icons.pages
                    onClicked: pageSheet.open()
                }
            }
        }
    }

    PaperMenu {
        id: addMenu

        PaperMenuItem {
            text: qsTr("Blank pages")
            onTriggered: chrome.canvas.addColumn("")
        }

        PaperMenuItem {
            text: qsTr("Import a PDF…")
            onTriggered: chrome.importRequested()
        }
    }

    PdfExportFlow {
        id: boardExport

        canvas: chrome.canvas
        onReported: message => chrome.reported(message)
    }

    PageSheet {
        id: pageSheet

        canvas: chrome.canvas
        onExportRequested: (columnId, title) => chrome.exportColumnRequested(columnId, title)
    }
}
