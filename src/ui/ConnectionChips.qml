pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// Small chips for the ways this device finds and reaches others (DHT, NOSTR, MQTT, PEERJS,
// TURN), each with a dot: filled when working, a ring when not. Opens the network settings.
AbstractButton {
    id: chips

    readonly property var entries: NetworkSummary.methods.concat(
        NetworkSummary.relay.count > 0 ? [NetworkSummary.relay] : [])

    implicitWidth: flow.implicitWidth
    implicitHeight: flow.implicitHeight
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Connection methods: %1. Open the network settings.")
        .arg(entries.map(e => e.kind.toUpperCase() + " " + (e.state === "working" ? qsTr("working")
             : e.state === "unreachable" ? qsTr("not reachable") : qsTr("checking"))).join(", "))
    ToolTip.text: qsTr("Network settings")
    ToolTip.visible: hovered
    ToolTip.delay: 600

    // Checks the servers now and then, so that the chips stay true.
    Timer {
        interval: 5 * 60 * 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: Network.checkServers()
    }

    contentItem: Flow {
        id: flow

        width: chips.width
        spacing: 5

        Repeater {
            model: chips.entries

            Rectangle {
                id: chip

                required property var modelData
                readonly property bool working: modelData.state === "working"

                implicitWidth: row.implicitWidth + 14
                implicitHeight: 22
                radius: 6
                color: working ? Theme.surface : "transparent"
                border.color: working ? Theme.border : "#CFCAC0"

                Row {
                    id: row

                    anchors.centerIn: parent
                    spacing: 4

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7
                        height: 7
                        radius: 4
                        color: chip.working ? Theme.working : "transparent"
                        border.width: chip.working ? 0 : 1.5
                        border.color: chip.modelData.state === "unreachable" ? Theme.unreachable : Theme.lineFaint
                    }

                    ServerKindLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        kind: chip.modelData.kind
                        size: 10
                        color: chip.working ? Theme.ink : Theme.quiet
                    }
                }
            }
        }
    }

    background: Rectangle {
        radius: 8
        color: chips.hovered ? Theme.hover : "transparent"
    }
}
