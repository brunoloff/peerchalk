pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → Network: whether PeerChalk goes online, and the servers it uses to find
// and reach peers. Saved settings apply the next time PeerChalk starts.
Item {
    id: pane

    // Browsers cannot join the DHT themselves, so they reach it through a proxy.
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    property string error

    function load() {
        const saved = Network.settings();
        enabledSwitch.checked = saved.enabled;
        bootstrapField.text = saved.bootstrap;
        dhtField.text = saved.dhtNetwork;
        stunField.text = saved.stun;
        turnField.text = saved.turn;
        proxyField.text = saved.dhtProxy;
        error = "";
    }

    function save() {
        error = Network.saveSettings({
            enabled: enabledSwitch.checked,
            bootstrap: bootstrapField.text,
            dhtNetwork: dhtField.text,
            stun: stunField.text,
            turn: turnField.text,
            dhtProxy: proxyField.text
        });
    }

    Component.onCompleted: load()

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        Panel {
            x: 28
            y: 28
            width: Math.min(640, scroll.availableWidth - 56)
            implicitHeight: column.implicitHeight + 48

            ColumnLayout {
                id: column

                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                SectionLabel {
                    text: qsTr("Connection")
                }

                NetworkStatus {
                    Layout.fillWidth: true
                }

                Switch {
                    id: enabledSwitch

                    objectName: "networkEnabled"
                    text: qsTr("Connect to other devices")
                }

                SettingField {
                    id: proxyField

                    visible: pane.inBrowser
                    label: qsTr("OpenDHT proxy")
                    hint: qsTr("The https:// server this browser finds peers through. Empty uses dhtproxy.jami.net.")
                    placeholder: "https://dhtproxy.jami.net"
                }

                SettingField {
                    id: bootstrapField

                    visible: !pane.inBrowser
                    label: qsTr("Bootstrap nodes")
                    hint: qsTr("OpenDHT nodes to start from, as host:port separated by commas. Empty uses bootstrap.jami.net:4222.")
                    placeholder: "bootstrap.jami.net:4222"
                }

                SettingField {
                    id: dhtField

                    label: qsTr("DHT network")
                    hint: qsTr("A number for a private OpenDHT network. Empty uses the public network.")
                    placeholder: "0"
                }

                SettingField {
                    id: stunField

                    label: qsTr("STUN server")
                    hint: qsTr("Helps reach people behind other routers, as host:port separated by commas. Empty uses Cloudflare's and Google's public servers.")
                    placeholder: "stun.cloudflare.com:3478, stun.l.google.com:19302"
                }

                SettingField {
                    id: turnField

                    label: qsTr("TURN server")
                    hint: qsTr("Relays traffic when no direct path exists, as user:password@host:port. The password is stored unencrypted on this device.")
                    placeholder: "user:password@turn.example.org:3478"
                    secret: true
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
                    visible: pane.error.length > 0 || Network.restartNeeded
                    wrapMode: Text.WordWrap
                    color: pane.error.length > 0 ? Theme.danger : Theme.muted
                    text: pane.error.length > 0 ? pane.error : qsTr("Saved. The changes apply the next time you start PeerChalk.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item {
                        Layout.fillWidth: true
                    }

                    PaperButton {
                        text: qsTr("Revert")
                        onClicked: pane.load()
                    }

                    PaperButton {
                        objectName: "saveNetworkSettings"
                        text: qsTr("Save")
                        primary: true
                        onClicked: pane.save()
                    }
                }
            }
        }
    }
}
