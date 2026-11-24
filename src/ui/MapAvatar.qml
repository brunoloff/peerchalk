import QtQuick
import QtQuick.Controls

// A connected person's initial in their colour on the connection map; a dashed ring while
// still connecting.
Control {
    id: avatar

    required property var device // from Network.devices
    property int size: 20
    readonly property bool connecting: device.route === "connecting"

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    Accessible.role: Accessible.StaticText
    Accessible.name: device.name

    background: Rectangle {
        radius: avatar.size / 2
        color: avatar.connecting ? "transparent"
             : Theme.personColors[avatar.device.colorIndex % Theme.personColors.length]
        border.width: avatar.connecting ? 1.5 : 0
        border.color: Theme.lineFaint
    }

    contentItem: Label {
        text: avatar.device.name.length > 0 ? avatar.device.name[0].toUpperCase() : "?"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Math.round(avatar.size * 0.45)
        font.weight: Font.DemiBold
        color: avatar.connecting ? Theme.quiet : "white"
    }
}
