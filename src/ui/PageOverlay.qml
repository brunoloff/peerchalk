pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Qt.labs.qmlmodels

// What a page board shows over its pages: subheadings (h2) printed on the board, page
// numbers, and in the gaps between pages the controls that add pages and subheadings. Text
// scales with the board; the controls keep their size. Column headings are the ruler's.
Item {
    id: overlay

    required property BoardCanvas canvas
    // Asks for a subheading's new text; the board page shows the dialog.
    signal renameSubheading(string columnId, string itemId, string text)
    // Asks for a new subheading's text, to add it before the item `before` ("" at the end).
    signal addSubheadingRequested(string columnId, string before)

    readonly property real offsetX: width / 2 - canvas.viewCenter.x * canvas.zoom
    readonly property real offsetY: height / 2 - canvas.viewCenter.y * canvas.zoom
    readonly property bool editable: !canvas.readOnly

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
                    roleValue: "subheading"

                    Text {
                        required property string label
                        required property real itemX
                        required property real itemY
                        required property real itemWidth
                        required property real itemHeight

                        x: itemX
                        y: itemY
                        width: itemWidth
                        height: itemHeight
                        text: label.length > 0 ? label : qsTr("Subheading")
                        font.family: Theme.headingFont
                        font.pixelSize: 30
                        font.weight: Font.Medium
                        color: label.length > 0 ? Theme.ink : Theme.muted
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                DelegateChoice {
                    roleValue: "page"

                    Text {
                        required property int number
                        required property real itemX
                        required property real itemY
                        required property real itemWidth
                        required property real itemHeight

                        x: itemX
                        y: itemY + itemHeight + 6
                        width: itemWidth
                        text: number
                        font.pixelSize: 14
                        color: Theme.muted
                        horizontalAlignment: Text.AlignRight
                    }
                }

                DelegateChoice {
                    Item {} // headings are shown by the column ruler; gaps below
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
                    id: subheading

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
                        onDoubleTapped: overlay.renameSubheading(subheading.columnId,
                                                                 subheading.itemId,
                                                                 subheading.label)
                    }

                    RailButton {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 28
                        implicitHeight: 28
                        visible: subheadingHover.hovered || subheadingMenu.visible
                        text: qsTr("Subheading menu")
                        iconPath: Icons.more
                        onClicked: subheadingMenu.popup()

                        Menu {
                            id: subheadingMenu

                            MenuItem {
                                text: qsTr("Rename…")
                                onTriggered: overlay.renameSubheading(subheading.columnId,
                                                                      subheading.itemId,
                                                                      subheading.label)
                            }

                            MenuItem {
                                text: qsTr("Remove subheading")
                                onTriggered: overlay.canvas.removePageItem(subheading.columnId,
                                                                           subheading.itemId)
                            }
                        }
                    }
                }
            }

            DelegateChoice {
                Item {}
            }
        }
    }
}
