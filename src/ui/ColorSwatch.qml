import QtQuick
import QtQuick.Controls

// One pen colour choice; used as a Repeater delegate over colour strings. The chosen
// colour shows a check mark; pale colours get an outline so they stand out on white.
AbstractButton {
    id: swatch

    required property string modelData
    required property BoardCanvas canvas
    readonly property bool current: Qt.colorEqual(canvas.penColor, modelData)
    readonly property color fill: modelData
    readonly property bool pale: 0.299 * fill.r + 0.587 * fill.g + 0.114 * fill.b > 0.75

    implicitWidth: 26
    implicitHeight: 26
    Accessible.name: qsTr("Colour %1").arg(modelData)
    Accessible.checked: current
    onClicked: canvas.penColor = modelData

    background: Rectangle {
        radius: width / 2
        color: swatch.fill
        border.width: swatch.pale || swatch.hovered ? 1 : 0
        border.color: swatch.hovered ? Theme.muted : Theme.border
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: 16
            height: 16
            visible: swatch.current
            path: Icons.check
            lineWidth: 2.5
            color: swatch.pale ? Theme.ink : "white"
        }
    }
}
