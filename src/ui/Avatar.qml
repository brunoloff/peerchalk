import QtQuick
import QtQuick.Controls

// A round initial on a colour, for a person.
Rectangle {
    id: avatar

    required property string name
    property int size: 36

    implicitWidth: size
    implicitHeight: size
    radius: size / 2
    color: Theme.personColors[0]

    Label {
        anchors.centerIn: parent
        text: Format.initial(avatar.name)
        color: "white"
        font.pixelSize: avatar.size * 0.42
        font.weight: Font.DemiBold
    }
}
