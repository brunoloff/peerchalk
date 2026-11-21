pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The right-hand panel with options for the current tool, and the minimap and zoom
// controls at its bottom. The header toggles auto-hide; when the panel floats over the
// board (auto-hide or a narrow window) it also offers a close button.
Rectangle {
    id: inspector

    required property BoardCanvas canvas
    required property bool autoHide
    property bool floating: false
    readonly property bool drawing: !pageTab && (canvas.tool === BoardCanvas.Pen
        || canvas.tool === BoardCanvas.Highlighter || canvas.tool === BoardCanvas.Text)
    // Page boards: the Page tab, about the current page and its column, is open.
    property bool pageTab: false
    readonly property bool showsPage: pageTab && canvas.pageBoard
    readonly property string toolName: {
        switch (inspector.canvas.tool) {
        case BoardCanvas.Pen: return qsTr("Pen");
        case BoardCanvas.Highlighter: return qsTr("Highlighter");
        case BoardCanvas.Eraser: return qsTr("Eraser");
        case BoardCanvas.Text: return qsTr("Text");
        default: return qsTr("Pan");
        }
    }

    signal autoHideToggled(bool enabled)
    signal closeRequested()
    signal exportColumnRequested(string columnId, string title)

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                visible: !inspector.canvas.pageBoard
                text: inspector.toolName
                font.family: Theme.headingFont
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            PaperSegments { // page boards: the tool's options, or the current page's
                objectName: "inspectorTabs"
                Layout.fillWidth: true
                visible: inspector.canvas.pageBoard
                stretch: true
                segmentHeight: 30
                choices: [{value: "tool", text: inspector.toolName},
                          {value: "page", text: qsTr("Page")}]
                value: inspector.pageTab ? "page" : "tool"
                onChosen: value => inspector.pageTab = value === "page"
            }

            RailButton {
                objectName: "autoHideButton"
                implicitWidth: 36
                implicitHeight: 36
                text: inspector.autoHide ? qsTr("Keep panel open") : qsTr("Auto-hide panel")
                iconPath: Icons.pin
                selected: !inspector.autoHide
                onClicked: inspector.autoHideToggled(!inspector.autoHide)
            }

            RailButton {
                visible: inspector.floating
                implicitWidth: 36
                implicitHeight: 36
                text: qsTr("Hide panel")
                iconPath: Icons.close
                onClicked: inspector.closeRequested()
            }
        }

        SectionLabel {
            visible: inspector.drawing
            text: qsTr("Colour")
        }

        ColorPalette {
            visible: inspector.drawing
            canvas: inspector.canvas
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
            visible: !inspector.drawing && !inspector.showsPage
            wrapMode: Text.WordWrap
            lineHeight: 1.3
            color: Theme.muted
            text: inspector.canvas.tool === BoardCanvas.Eraser
                ? qsTr("Drag across strokes to remove them. Undo brings back everything erased in one drag.")
                : qsTr("Drag to move around the board. With any tool you can also drag with the right or middle mouse button, scroll to pan, or hold Ctrl and scroll to zoom.")
        }

        PageInspector {
            objectName: "pageInspector"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: inspector.showsPage
            canvas: inspector.canvas
            onExportRequested: (columnId, title) => inspector.exportColumnRequested(columnId, title)
        }

        Item {
            Layout.fillHeight: true
            visible: !inspector.showsPage
        }

        NavigatorPanel {
            objectName: "panelNavigator"
            Layout.fillWidth: true
            visible: !inspector.showsPage
            canvas: inspector.canvas
            stretch: true
        }
    }
}
