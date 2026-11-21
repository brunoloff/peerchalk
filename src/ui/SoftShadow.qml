pragma ComponentBehavior: Bound

import QtQuick

// A soft shadow under a rounded surface of the same size, from a few faint outlines, so it
// draws the same with every scene graph backend. Put it first, under the surface.
Item {
    id: shadow

    property real radius: Theme.panelRadius
    property real spread: 10 // how far the shadow reaches below
    property real strength: 0.16

    Repeater {
        model: 8

        Rectangle {
            required property int index
            readonly property real step: (index + 1) / 8

            x: -shadow.spread * step * 0.5
            y: shadow.spread * step * 0.6
            width: shadow.width + shadow.spread * step
            height: shadow.height + shadow.spread * step * 0.5
            radius: shadow.radius + shadow.spread * step * 0.5
            color: Qt.rgba(0.11, 0.106, 0.098, shadow.strength / 8 * (1.1 - step))
        }
    }
}
