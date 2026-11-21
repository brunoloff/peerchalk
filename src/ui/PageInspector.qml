pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The side panel's Page tab on a page board: the current page, and settings of its column
// for pages added from now on, which can also be given to every page of the column.
ColumnLayout {
    id: panel

    required property BoardCanvas canvas
    readonly property var page: canvas.currentPage
    readonly property bool hasPage: page.pageId !== undefined
    readonly property var column: canvas.pageColumns.find(entry => entry.columnId === page.columnId) ?? null
    readonly property bool editable: !canvas.readOnly

    signal exportRequested(string columnId, string title)

    spacing: 14

    function millimetres(units: real): int {
        return Math.round(units * 25.4 / 96);
    }

    function sizeText(width: real, height: real): string {
        return qsTr("%1 × %2 mm · %3").arg(millimetres(width)).arg(millimetres(height))
            .arg(width > height ? qsTr("landscape") : qsTr("portrait"));
    }

    Label {
        Layout.fillWidth: true
        visible: !panel.hasPage
        wrapMode: Text.WordWrap
        color: Theme.muted
        text: qsTr("Move a page into view to see it here.")
    }

    SectionLabel {
        visible: panel.hasPage
        text: qsTr("Page %1 of %2").arg(panel.page.number ?? 0).arg(panel.page.count ?? 0)
    }

    Rectangle { // what the page shows
        Layout.fillWidth: true
        visible: panel.hasPage
        implicitHeight: pageRow.implicitHeight + 20
        radius: Theme.controlRadius
        border.color: Theme.border

        RowLayout {
            id: pageRow

            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Rectangle {
                implicitWidth: 32
                implicitHeight: 40
                radius: 4
                color: (panel.page.pdfPage ?? 0) > 0 ? "#FBEAE5" : "#F1EFEA"

                Label {
                    anchors.centerIn: parent
                    text: (panel.page.pdfPage ?? 0) > 0 ? "PDF" : ""
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: "#B42318"
                }

                PageSwatch {
                    anchors.centerIn: parent
                    visible: (panel.page.pdfPage ?? 0) === 0
                    implicitWidth: 20
                    implicitHeight: 26
                    pattern: panel.column?.background ?? "blank"
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: (panel.page.pdfPage ?? 0) > 0
                        ? (panel.page.columnTitle.length > 0 ? panel.page.columnTitle : qsTr("PDF"))
                        : qsTr("Blank page")
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    color: Theme.ink
                }

                Label {
                    Layout.fillWidth: true
                    text: (panel.page.pdfPage ?? 0) > 0
                        ? qsTr("Its page %1 · %2, as in the PDF").arg(panel.page.pdfPage)
                              .arg(panel.sizeText(panel.page.width, panel.page.height))
                        : panel.sizeText(panel.page.width ?? 0, panel.page.height ?? 0)
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: panel.column !== null
        implicitHeight: 1
        color: Theme.border
    }

    SectionLabel {
        Layout.fillWidth: true
        visible: panel.column !== null
        text: qsTr("Column · %1").arg(panel.column?.title.length > 0 ? panel.column.title : qsTr("untitled"))
        elide: Text.ElideRight
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.column !== null && panel.editable
        spacing: 8

        Label {
            text: qsTr("Paper for new pages")
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.ink
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: ["A4", "Letter", "A5", "Legal"]

                ChoiceChip {
                    required property string modelData

                    implicitHeight: 32
                    text: modelData
                    checked: panel.column?.paper === modelData
                    onClicked: panel.canvas.setColumnPaper(panel.column.columnId, modelData,
                                                           panel.column.landscape, false)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: (panel.column?.paper ?? "").length > 0
                    ? qsTr("%1 · %2").arg(panel.column.paper)
                          .arg(panel.column.landscape ? qsTr("landscape") : qsTr("portrait"))
                    : qsTr("A size of its own")
                font.pixelSize: 12
                color: Theme.muted
            }

            PaperSegments {
                visible: (panel.column?.paper ?? "").length > 0
                segmentHeight: 24
                choices: [{value: "portrait", text: qsTr("Portrait")},
                          {value: "landscape", text: qsTr("Landscape")}]
                value: panel.column?.landscape ? "landscape" : "portrait"
                onChosen: value => panel.canvas.setColumnPaper(
                              panel.column.columnId, panel.column.paper, value === "landscape", false)
            }
        }

        Label {
            Layout.topMargin: 6
            text: qsTr("Background for new pages")
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.ink
        }

        BackgroundTiles {
            Layout.fillWidth: true
            tileHeight: 54
            columnSpacing: 6
            value: panel.column?.background ?? ""
            onChosen: value => panel.canvas.setColumnBackground(panel.column.columnId, value)
        }

        PaperButton {
            Layout.fillWidth: true
            Layout.topMargin: 4
            font.pixelSize: 13
            enabled: (panel.column?.paper ?? "").length > 0
            text: qsTr("Apply to all pages in this column")
            onClicked: panel.canvas.setColumnPaper(panel.column.columnId, panel.column.paper,
                                                   panel.column.landscape, true)
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("PDF pages keep the size their PDF gives them.")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.muted
        }
    }

    Item {
        Layout.fillHeight: true
    }

    PaperButton {
        Layout.fillWidth: true
        visible: panel.column !== null
        font.pixelSize: 14
        text: qsTr("Export this column as PDF")
        onClicked: panel.exportRequested(panel.column.columnId, panel.column.title)
    }

    PaperButton {
        Layout.fillWidth: true
        visible: panel.hasPage && panel.editable
        flat: true
        destructive: true
        font.pixelSize: 14
        text: qsTr("Delete page %1").arg(panel.page.number ?? 0)
        onClicked: panel.canvas.removePageItem(panel.page.columnId, panel.page.pageId)
    }
}
