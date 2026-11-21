pragma ComponentBehavior: Bound

import QtQuick
import Qt.labs.qmlmodels

// What a page board shows over its pages: each column's heading (h1) with its pages and
// paper, subheadings (h2) with a rule after them, page numbers, and in the gaps the controls
// that add pages and subheadings. Printed text scales with the board; controls keep their
// size.
Item {
    id: overlay

    required property BoardCanvas canvas
    property bool touch: false // no hover: the gaps show their controls all the time
    // Asks for a subheading's new text; the board page shows the dialog.
    signal renameSubheading(string columnId, string itemId, string text)
    // Asks for a new subheading's text, to add it before the item `before` ("" at the end).
    signal addSubheadingRequested(string columnId, string before)

    readonly property real offsetX: width / 2 - canvas.viewCenter.x * canvas.zoom
    readonly property real offsetY: height / 2 - canvas.viewCenter.y * canvas.zoom
    readonly property bool editable: !canvas.readOnly

    function column(columnId: string): var {
        return canvas.pageColumns.find(entry => entry.columnId === columnId) ?? null;
    }

    function backgroundName(value: string): string {
        return {blank: qsTr("blank"), ruled: qsTr("ruled"), squared: qsTr("squared"),
                dotted: qsTr("dotted")}[value] ?? "";
    }

    // "A4 · ruled": what a new page of the column gets.
    function paperOf(columnId: string): string {
        const entry = column(columnId);
        if (entry === null)
            return "";
        const paper = entry.paper.length > 0 ? entry.paper : qsTr("Custom size");
        return qsTr("%1 · %2").arg(entry.landscape ? qsTr("%1 landscape").arg(paper) : paper)
                              .arg(backgroundName(entry.background));
    }

    // Printed text, in board coordinates.
    Item {
        transform: Matrix4x4 {
            matrix: overlay.canvas.viewMatrix
        }

        Repeater {
            model: overlay.canvas.pageItems

            delegate: DelegateChooser {
                role: "kind"

                DelegateChoice {
                    roleValue: "heading"

                    Item {
                        id: heading

                        required property string columnId
                        required property string label
                        required property real itemX
                        required property real itemY
                        required property real itemWidth

                        x: itemX
                        y: itemY
                        width: itemWidth

                        Text {
                            y: 6
                            width: parent.width
                            text: heading.label.length > 0 ? heading.label : qsTr("Untitled column")
                            font.family: Theme.headingFont
                            font.pixelSize: 52
                            font.weight: Font.DemiBold
                            color: heading.label.length > 0 ? Theme.ink : Theme.muted
                            elide: Text.ElideRight
                        }

                        Text {
                            y: 74
                            width: parent.width
                            readonly property var entry: overlay.column(heading.columnId)
                            text: entry === null ? ""
                                : (entry.pages === 1 ? qsTr("1 page") : qsTr("%1 pages").arg(entry.pages))
                                  + " · " + overlay.paperOf(heading.columnId)
                            font.pixelSize: 24
                            color: Theme.muted
                            elide: Text.ElideRight
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "subheading"

                    Item {
                        id: subheading

                        required property string label
                        required property real itemX
                        required property real itemY
                        required property real itemWidth
                        required property real itemHeight

                        x: itemX
                        y: itemY
                        width: itemWidth
                        height: itemHeight

                        Text {
                            id: subheadingText

                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, parent.width * 0.85)
                            text: subheading.label.length > 0 ? subheading.label : qsTr("Subheading")
                            font.family: Theme.headingFont
                            font.pixelSize: 32
                            font.weight: Font.DemiBold
                            color: subheading.label.length > 0 ? Theme.ink : Theme.muted
                            elide: Text.ElideRight
                        }

                        Rectangle { // the rule after it
                            anchors.left: subheadingText.right
                            anchors.leftMargin: 18
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 2
                            color: "#B9B4AA"
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "page"

                    Item {
                        id: pageLabels

                        required property int number
                        required property bool waiting
                        required property real itemX
                        required property real itemY
                        required property real itemWidth
                        required property real itemHeight

                        x: itemX
                        y: itemY
                        width: itemWidth
                        height: itemHeight

                        Text { // the page number, in the gap below the page
                            anchors.top: parent.bottom
                            anchors.topMargin: 6
                            width: parent.width
                            text: pageLabels.number
                            font.pixelSize: 16
                            color: "#8F8A80"
                            horizontalAlignment: Text.AlignHCenter
                        }

                    }
                }

                DelegateChoice {
                    Item {} // gaps are controls, below
                }
            }
        }
    }

    // Controls, in screen coordinates.
    Repeater {
        model: overlay.canvas.pageItems

        delegate: DelegateChooser {
            role: "kind"

            DelegateChoice {
                roleValue: "gap"

                PageGapControls {
                    canvas: overlay.canvas
                    editable: overlay.editable
                    alwaysShown: overlay.touch
                    paper: atEnd ? overlay.paperOf(columnId) : ""
                    x: itemX * overlay.canvas.zoom + overlay.offsetX
                    y: itemY * overlay.canvas.zoom + overlay.offsetY
                    width: itemWidth * overlay.canvas.zoom
                    height: itemHeight * overlay.canvas.zoom
                    onSubheadingRequested: overlay.addSubheadingRequested(columnId, itemId)
                }
            }

            DelegateChoice {
                roleValue: "subheading"

                Item {
                    id: subheadingControls

                    required property string columnId
                    required property string itemId
                    required property string label
                    required property real itemX
                    required property real itemY
                    required property real itemWidth
                    required property real itemHeight

                    x: itemX * overlay.canvas.zoom + overlay.offsetX
                    y: itemY * overlay.canvas.zoom + overlay.offsetY
                    width: itemWidth * overlay.canvas.zoom
                    height: itemHeight * overlay.canvas.zoom
                    visible: overlay.editable && height >= 16

                    HoverHandler {
                        id: subheadingHover
                    }

                    TapHandler {
                        onDoubleTapped: overlay.renameSubheading(subheadingControls.columnId,
                                                                 subheadingControls.itemId,
                                                                 subheadingControls.label)
                    }

                    RailButton {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 28
                        implicitHeight: 28
                        visible: subheadingHover.hovered || subheadingMenu.visible
                        text: qsTr("Subheading menu")
                        iconPath: Icons.more
                        onClicked: subheadingMenu.popup(this, 0, height + 4)

                        PaperMenu {
                            id: subheadingMenu

                            PaperMenuItem {
                                text: qsTr("Rename…")
                                onTriggered: overlay.renameSubheading(subheadingControls.columnId,
                                                                      subheadingControls.itemId,
                                                                      subheadingControls.label)
                            }

                            PaperMenuItem {
                                text: qsTr("Remove subheading")
                                destructive: true
                                onTriggered: overlay.canvas.removePageItem(
                                                 subheadingControls.columnId,
                                                 subheadingControls.itemId)
                            }
                        }
                    }
                }
            }

            DelegateChoice {
                roleValue: "page"

                PdfWaitingPill {
                    required property bool waiting
                    required property string blob
                    required property real itemX
                    required property real itemY
                    required property real itemWidth
                    required property real itemHeight

                    // The middle of the part of the page on screen.
                    readonly property real shownTop: Math.max(0, itemY * overlay.canvas.zoom + overlay.offsetY)
                    readonly property real shownBottom: Math.min(overlay.height,
                        (itemY + itemHeight) * overlay.canvas.zoom + overlay.offsetY)

                    visible: waiting && itemWidth * overlay.canvas.zoom > 160 && shownBottom - shownTop > 60
                    hash: blob
                    x: (itemX + itemWidth / 2) * overlay.canvas.zoom + overlay.offsetX - width / 2
                    y: (shownTop + shownBottom) / 2 - height / 2
                }
            }

            DelegateChoice {
                Item {}
            }
        }
    }
}
