import QtQuick
import QtQuick.Controls

// One preset pen width; used as a Repeater delegate over widths.
AbstractButton {
    id: button

    required property real modelData
    required property BoardCanvas canvas
    readonly property bool current: canvas.penWidth === modelData

    implicitWidth: 36
    implicitHeight: 36
    Accessible.name: qsTr("Width %1").arg(modelData)
    Accessible.checked: current
    onClicked: canvas.penWidth = modelData

    background: Rectangle {
        radius: 8
        color: button.current ? Theme.accentSoft : button.hovered ? Theme.hover : "transparent"
    }

    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(button.modelData * 1.6 + 2, 16)
            height: width
            radius: width / 2
            color: Theme.ink
        }
    }
}
