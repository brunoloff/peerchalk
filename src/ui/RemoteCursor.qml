import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// Another person's pointer on the board, with their name; a Repeater delegate over
// Network.boardPresence entries.
Item {
    id: cursor

    required property var modelData
    required property BoardCanvas boardCanvas
    readonly property color tint: Theme.personColors[modelData.colorIndex % Theme.personColors.length]
    // Depends on the view, so the cursor follows panning and zooming.
    readonly property point at: {
        boardCanvas.zoom;
        boardCanvas.viewCenter;
        return boardCanvas.toScreen(Qt.point(modelData.x, modelData.y));
    }

    x: at.x
    y: at.y
    visible: modelData.cursor
    z: 10

    Shape {
        width: 20
        height: 22
        layer.enabled: true
        layer.samples: 4

        ShapePath {
            fillColor: cursor.tint
            strokeColor: "white"
            strokeWidth: 1.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M2 2l16 8-7 2-3 8z"
            }
        }
    }

    Rectangle {
        x: 14
        y: 18
        width: label.implicitWidth + 16
        height: label.implicitHeight + 6
        radius: height / 2
        color: cursor.tint

        Label {
            id: label

            anchors.centerIn: parent
            text: cursor.modelData.name
            color: "white"
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }
}
