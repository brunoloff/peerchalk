import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A kind of network server as a small icon and its name in capitals: DHT, NOSTR, MQTT,
// PEERJS, STUN or TURN.
RowLayout {
    id: label

    required property string kind
    property color color: Theme.ink
    property int size: 11

    spacing: 6

    Icon {
        Layout.preferredWidth: label.size + 5
        Layout.preferredHeight: label.size + 5
        path: Icons[label.kind] ?? Icons.relay
        color: label.color
        lineWidth: 1.9
    }

    Label {
        text: label.kind.toUpperCase()
        font.family: Theme.headingFont
        font.pixelSize: label.size
        font.weight: Font.DemiBold
        font.letterSpacing: label.size * 0.07
        color: label.color
    }
}
