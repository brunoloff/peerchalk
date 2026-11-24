import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The network settings: going online, the server lists and the DHT network, with notes on
// environment variables and on changes waiting for a restart.
ColumnLayout {
    id: column

    property string error
    property bool advancedOpen: false

    function save(enabled, dhtNetwork) {
        error = Network.saveSettings({ enabled: enabled, dhtNetwork: dhtNetwork });
    }

    spacing: 28

    ColumnLayout {
        spacing: 6

        Label {
            text: qsTr("Network")
            font.family: Theme.headingFont
            font.pixelSize: 30
            font.weight: Font.Bold
            color: Theme.ink
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            color: Theme.muted
            text: qsTr("How PeerChalk finds your other devices and the people you share boards with, and how it reaches them.")
        }

        Label {
            Layout.fillWidth: true
            visible: Network.settingsOverridden
            wrapMode: Text.WordWrap
            color: Theme.danger
            text: qsTr("PEERCHALK_* environment variables are set and take precedence over these settings.")
        }

        Label {
            Layout.fillWidth: true
            visible: column.error.length > 0 || Network.restartNeeded
            wrapMode: Text.WordWrap
            color: column.error.length > 0 ? Theme.danger : Theme.accentInk
            text: column.error.length > 0 ? column.error
                                          : qsTr("Saved. The changes apply the next time you start PeerChalk.")
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: onlineRow.implicitHeight + 28
        radius: 12
        color: Theme.surface
        border.color: Theme.border

        RowLayout {
            id: onlineRow

            anchors.fill: parent
            anchors.margins: 14
            anchors.leftMargin: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Connect to other devices")
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.ink
                }

                Label {
                    text: qsTr("Off keeps everything on this device.")
                    font.pixelSize: 13
                    color: Theme.muted
                }
            }

            Switch {
                id: enabledSwitch

                objectName: "networkEnabled"
                Accessible.name: qsTr("Connect to other devices")
                Component.onCompleted: checked = Network.settings().enabled
                onToggled: column.save(checked, dhtField.text)
            }
        }
    }

    ServerListSection {
        objectName: "discoveryServers"
        Layout.fillWidth: true
        group: "discovery"
        kinds: ["dht", "nostr", "mqtt", "peerjs"]
        title: qsTr("Finding other devices")
        description: qsTr("All of these are used at once. Two devices meet through any one they both use.")
    }

    ServerListSection {
        objectName: "firewallServers"
        Layout.fillWidth: true
        group: "firewall"
        kinds: ["stun", "turn"]
        title: qsTr("Getting through firewalls")
        description: qsTr("STUN finds a direct way between devices. When there is none, a TURN relay passes the encrypted data along.")
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        Button {
            id: advancedButton

            Layout.fillWidth: true
            flat: true
            text: qsTr("Advanced: DHT network")
            Accessible.name: text
            onClicked: column.advancedOpen = !column.advancedOpen

            contentItem: Label {
                text: advancedButton.text + (column.advancedOpen ? "  ▴" : "  ▾")
                font.pixelSize: 14
                color: Theme.muted
            }

            background: Rectangle {
                radius: 12
                color: "transparent"
                border.color: "#D9D5CC"
            }
        }

        SettingField {
            id: dhtField

            Layout.fillWidth: true
            visible: column.advancedOpen
            label: qsTr("DHT network")
            hint: qsTr("Only devices on the same OpenDHT network meet. Empty is the public network.")
            placeholder: "0"
            Component.onCompleted: text = Network.settings().dhtNetwork
        }

        PaperButton {
            Layout.alignment: Qt.AlignRight
            visible: column.advancedOpen
            text: qsTr("Save")
            primary: true
            onClicked: column.save(enabledSwitch.checked, dhtField.text)
        }
    }
}
