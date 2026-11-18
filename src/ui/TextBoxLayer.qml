pragma ComponentBehavior: Bound

import QtQuick

// The board's text boxes, over its strokes. The boxes live in world coordinates; the frame
// and controls of the one being edited stay the same size at any zoom.
Item {
    id: layer

    required property BoardCanvas canvas
    property TextBoxView activeView: null

    Item {
        transform: Matrix4x4 {
            matrix: layer.canvas.viewMatrix
        }

        Repeater {
            model: layer.canvas.textBoxes

            delegate: TextBoxView {
                canvas: layer.canvas
                onEditingChanged: {
                    if (editing)
                        layer.activeView = this;
                    else if (layer.activeView === this)
                        layer.activeView = null;
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: layer.activeView !== null

        sourceComponent: TextBoxChrome {
            canvas: layer.canvas
            view: layer.activeView
        }
    }
}
