pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A list of servers on the network settings page ("discovery" or "firewall"): one row each,
// with buttons that add a server, after a check, and reset the list to PeerChalk's defaults.
ColumnLayout {
    id: section

    required property string group
    required property string title
    required property string description
    required property var kinds // the kinds one can add here
    property var servers: []
    property string error
    property bool adding: false
    property bool confirmingReset: false

    function reload() {
        servers = Network.servers(group);
    }

    function placeholderFor(kind) {
        const inBrowser = Qt.platform.os === "wasm";
        return {
            "dht": inBrowser ? "https://dht-proxy.example" : "dht.example.com:4222",
            "nostr": "wss://relay.example",
            "mqtt": "wss://broker.example:8884/mqtt",
            "peerjs": "wss://peerjs.example/peerjs",
            "stun": "stun.example.com:3478",
            "turn": "user:password@turn.example.com:3478"
        }[kind];
    }

    function finish(problem) {
        error = problem;
        if (problem.length === 0) {
            adding = false;
            confirmingReset = false;
            reload();
            Network.checkServers();
        }
    }

    spacing: 10
    Component.onCompleted: reload()

    Label {
        text: section.title
        font.family: Theme.headingFont
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: Theme.ink
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: -6
        text: section.description
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        color: Theme.muted
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: rows.implicitHeight + 2
        radius: 12
        color: Theme.surface
        border.color: Theme.border
        clip: true

        ColumnLayout {
            id: rows

            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            Repeater {
                model: section.servers

                ServerRow {
                    required property var modelData

                    Layout.fillWidth: true
                    server: modelData
                    result: Network.serverStatus[modelData.kind + " " + modelData.address]
                    onRemoveRequested: index => section.finish(Network.removeServer(section.group, index))
                }
            }

            ServerAddForm {
                Layout.fillWidth: true
                visible: section.adding
                kinds: section.kinds
                placeholderFor: section.placeholderFor
                onAddRequested: (kind, address) => section.finish(Network.addServer(kind, address))
                onCanceled: {
                    section.adding = false;
                    section.error = "";
                }
            }

            ServerListFooter {
                Layout.fillWidth: true
                adding: section.adding
                confirmingReset: section.confirmingReset
                onAddRequested: section.adding = true
                onResetAsked: section.confirmingReset = true
                onResetConfirmed: section.finish(Network.resetServers(section.group))
                onResetCanceled: section.confirmingReset = false
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: section.error.length > 0
        text: section.error
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        color: Theme.danger
    }

    Connections {
        target: Network

        function onSettingsChanged() {
            section.reload();
        }
    }
}
