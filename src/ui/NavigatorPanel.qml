import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The minimap and zoom controls. Floating, they hug the right edge at a fixed size;
// with stretch set (inside the side panel) they fill the available width.
ColumnLayout {
    id: navigator

    required property BoardCanvas canvas
    property bool stretch: false

    spacing: 8

    Panel {
        Layout.alignment: Qt.AlignRight
        Layout.fillWidth: navigator.stretch
        implicitWidth: 180
        implicitHeight: navigator.stretch ? 150 : 120
        radius: 12

        BoardMinimap {
            anchors.fill: parent
            anchors.margins: 6
            canvas: navigator.canvas
            viewportColor: Theme.accent
            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Minimap. Click to move the view there.")
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
                text: qsTr("Fit all strokes (Shift+1)")
                iconPath: Icons.fit
                enabled: navigator.canvas.strokeCount > 0
                onClicked: navigator.canvas.fitContent()
            }
        }
    }
}
