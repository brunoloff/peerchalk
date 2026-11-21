import QtQuick
import QtQuick.Shapes

// A rounded rectangle drawn with a dashed line, for places where something can be added.
Shape {
    id: frame

    property color color: "#B9B4AA"
    property real radius: 8
    property real lineWidth: 1.5
    property color fill: "transparent"

    // It takes the size of what it frames; a size of its own would feed back into that.
    implicitWidth: 0
    implicitHeight: 0
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: frame.color
        strokeWidth: frame.lineWidth
        strokeStyle: ShapePath.DashLine
        dashPattern: [3, 2.5]
        fillColor: frame.fill

        PathRectangle {
            x: frame.lineWidth / 2
            y: frame.lineWidth / 2
            width: frame.width - frame.lineWidth
            height: frame.height - frame.lineWidth
            radius: frame.radius
        }
    }
}
