import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One line about the connection: online or not, and how many devices are connected.
RowLayout {
    spacing: 8

    Rectangle {
        implicitWidth: 8
        implicitHeight: 8
        radius: 4
        color: Network.online ? "#16A34A" : Theme.muted
    }

    Label {
        Layout.fillWidth: true
        elide: Text.ElideRight
        font.pixelSize: 12
        color: Theme.muted
        text: Network.updateNeeded ? qsTr("Update PeerChalk: someone here uses a newer version")
            : Network.connectedDevices === 1 ? qsTr("Online · 1 device connected")
            : Network.connectedDevices > 1 ? qsTr("Online · %1 devices connected").arg(Network.connectedDevices)
            : Network.status
    }
}
