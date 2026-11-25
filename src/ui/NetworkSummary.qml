pragma Singleton

import QtQuick

// The ways this device finds others, as the connection map and the sidebar chips show them:
// one entry per kind of discovery server this version uses, with whether any of them
// answers, then the TURN relay. The DHT counts as working on desktops once the node is online.
QtObject {
    id: summary

    property int settingsRevision: 0
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    // [{kind, state, count}] in a fixed order; state is "working", "checking", "unreachable" or
    // "offline" (the desktop DHT while this device is not online).
    readonly property var methods: summary.collect(Network.serverStatus, Network.online, settingsRevision)
    // {kind: "turn", state, count}; count 0 when the relay is turned off.
    readonly property var relay: summary.relayState(Network.serverStatus, settingsRevision)

    function relayState(status, revision) {
        return kindState(Network.servers("firewall").filter(s => s.kind === "turn"), status);
    }

    function kindState(servers, status) {
        const states = servers.map(s => (status[s.kind + " " + s.address] ?? {}).state ?? "checking");
        return {
            kind: "turn",
            count: servers.length,
            state: states.includes("working") ? "working"
                 : states.length > 0 && states.every(s => s === "unreachable") ? "unreachable" : "checking"
        };
    }

    function collect(status, online, revision) {
        const servers = Network.servers("discovery");
        const found = [];
        for (const kind of ["dht", "nostr", "mqtt", "peerjs"]) {
            const ofKind = servers.filter(s => s.kind === kind);
            if (ofKind.length === 0)
                continue;
            const entry = kindState(ofKind, status);
            entry.kind = kind;
            // On desktops the DHT works once the node is online; name lookups only show that
            // its nodes exist.
            if (kind === "dht" && !inBrowser && entry.state !== "unreachable")
                entry.state = online ? "working" : "offline";
            found.push(entry);
        }
        return found;
    }

    // "2 nodes", "1 relay"…
    function countText(entry) {
        const words = {
            "dht": inBrowser ? [qsTr("proxy"), qsTr("proxies")] : [qsTr("node"), qsTr("nodes")],
            "nostr": [qsTr("relay"), qsTr("relays")],
            "mqtt": [qsTr("broker"), qsTr("brokers")],
            "peerjs": [qsTr("server"), qsTr("servers")],
            "turn": [qsTr("relay"), qsTr("relays")]
        }[entry.kind];
        return entry.count + " " + words[entry.count === 1 ? 0 : 1];
    }

    property Connections watcher: Connections {
        target: Network

        function onSettingsChanged() {
            summary.settingsRevision += 1;
        }
    }
}
