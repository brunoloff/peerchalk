import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The minimap and zoom controls; page boards can show their outline in place of the map.
// Floating, they hug the right edge at a fixed size; with stretch set (inside the side panel)
// they fill the available width.
ColumnLayout {
    id: navigator

    required property BoardCanvas canvas
    property bool stretch: false
    property bool outline: false // page boards: the outline tab is open
    readonly property bool showsOutline: outline && canvas.pageBoard

    spacing: 8

    Panel {
        Layout.alignment: Qt.AlignRight
        Layout.fillWidth: navigator.stretch
        implicitWidth: navigator.canvas.pageBoard ? 236 : 180
        implicitHeight: (navigator.showsOutline ? 230 : navigator.stretch ? 150 : 120)
                        + (navigator.canvas.pageBoard ? 36 : 0)
        radius: 12

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            PaperSegments {
                Layout.fillWidth: true
                visible: navigator.canvas.pageBoard
                stretch: true
                choices: [{value: "map", text: qsTr("Map")},
                          {value: "outline", text: qsTr("Outline")}]
                value: navigator.outline ? "outline" : "map"
                onChosen: value => navigator.outline = value === "outline"
            }

            BoardMinimap {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !navigator.showsOutline
                canvas: navigator.canvas
                viewportColor: Theme.accent
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("Minimap. Click to move the view there.")
            }

            PageOutline {
                objectName: "pageOutline"
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: navigator.showsOutline
                canvas: navigator.canvas
            }
        }
    }

    Panel {
        Layout.alignment: Qt.AlignRight
        Layout.fillWidth: navigator.stretch
        implicitWidth: zoomRow.implicitWidth
        implicitHeight: 40
        radius: 12

        RowLayout {
            id: zoomRow

            anchors.fill: parent
            spacing: 0

            RailButton {
                implicitHeight: 40
                text: qsTr("Zoom out (Ctrl+-)")
                iconPath: Icons.minus
                onClicked: navigator.canvas.zoomBy(1 / 1.25)
            }

            Label {
                Layout.fillWidth: navigator.stretch
                Layout.minimumWidth: 54
                horizontalAlignment: Text.AlignHCenter
                text: Math.round(navigator.canvas.zoom * 100) + "%"
                color: Theme.ink
            }

            RailButton {
                implicitHeight: 40
                text: qsTr("Zoom in (Ctrl+=)")
                iconPath: Icons.plus
                onClicked: navigator.canvas.zoomBy(1.25)
            }

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 1
                color: Theme.border
            }

            RailButton {
                implicitHeight: 40
                text: navigator.canvas.pageBoard ? qsTr("Fit all pages (Shift+1)")
                                                 : qsTr("Fit all strokes (Shift+1)")
                iconPath: Icons.fit
                enabled: navigator.canvas.strokeCount > 0 || navigator.canvas.pageBoard
                onClicked: navigator.canvas.fitContent()
            }
        }
    }
}
