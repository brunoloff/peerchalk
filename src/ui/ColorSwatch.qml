import QtQuick
import QtQuick.Controls

// One pen colour choice; used as a Repeater delegate over colour strings.
AbstractButton {
    id: swatch

    required property string modelData
    required property BoardCanvas canvas
    readonly property bool current: Qt.colorEqual(canvas.penColor, modelData)

    implicitWidth: 32
    implicitHeight: 32
    Accessible.name: qsTr("Colour %1").arg(modelData)
    Accessible.checked: current
    onClicked: canvas.penColor = modelData

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: swatch.current ? 2 : 0
            border.color: Theme.accent
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: swatch.current ? 4 : 0
            radius: width / 2
            color: swatch.modelData
        }
    }
}
