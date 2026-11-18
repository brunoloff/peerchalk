import QtQuick
import QtQuick.Shapes

// A line icon from SVG path data on a 24x24 grid, drawn at the item's size.
Item {
    id: root

    required property string path
    property color color: Theme.ink
    property real lineWidth: 1.75
    readonly property real gridScale: Math.min(width, height) / 24

    implicitWidth: 20
    implicitHeight: 20

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 4
        layer.smooth: true
        // Drawn at several times the screen size, so that edges stay crisp.
        layer.textureSize: Qt.size(Math.ceil(width * 3), Math.ceil(height * 3))

        ShapePath {
            scale: Qt.size(root.gridScale, root.gridScale)
            strokeColor: root.color
            strokeWidth: root.lineWidth * root.gridScale
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.path
            }
        }
    }
}
