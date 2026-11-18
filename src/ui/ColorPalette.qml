pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// The pen colours: the default palette, then colours you picked yourself, then a
// button that opens a colour picker for a new one.
Grid {
    id: palette

    required property BoardCanvas canvas

    // "#RRGGBB" for a colour, ignoring its alpha.
    function hex(color: color): string {
        return "#" + [color.r, color.g, color.b].map(v => Math.round(v * 255).toString(16).padStart(2, "0")).join("").toUpperCase();
    }

    columns: Theme.penColorColumns
    spacing: 4

    Repeater {
        model: Theme.penColors

        delegate: ColorSwatch {
            canvas: palette.canvas
        }
    }

    Repeater {
        model: Library.customColors

        delegate: ColorSwatch {
            canvas: palette.canvas
        }
    }

    AbstractButton {
        id: addButton

        objectName: "customColorButton"
        implicitWidth: 26
        implicitHeight: 26
        Accessible.name: qsTr("Custom colour…")
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: {
            picker.selectedColor = palette.canvas.penColor;
            picker.open();
        }

        background: Rectangle {
            radius: width / 2
            color: addButton.hovered ? Theme.hover : Theme.background
            border.color: Theme.border
        }

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                path: Icons.plus
            }
        }
    }

    ColorDialog {
        id: picker

        title: qsTr("Custom colour")
        onAccepted: {
            const color = palette.hex(picker.selectedColor);
            Library.addCustomColor(color);
            palette.canvas.penColor = color;
        }
    }
}
