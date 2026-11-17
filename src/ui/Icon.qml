import QtQuick
import QtQuick.Shapes

// A stroke icon from SVG path data on a 24x24 grid, scaled to the item's size.
Item {
    id: root

    required property string path
    property color color: Theme.ink
    property real lineWidth: 2

    implicitWidth: 20
    implicitHeight: 20

    Shape {
        width: 24
        height: 24
        scale: root.width / 24
        transformOrigin: Item.TopLeft
        layer.enabled: true
        layer.samples: 4

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.path
            }
        }
    }
}
