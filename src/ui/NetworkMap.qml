pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The connection map on the network page: the ways this device finds others, joined to it,
// and the devices it is connected to, grouped by how (directly or through a relay) so that
// many fit. Tall (methods above, devices below) or wide (methods left, devices right).
Item {
    id: map

    property bool wide: width >= 720
    readonly property int margin: 28
    property var methods: NetworkSummary.methods // the live ones unless given
    property var devices: Network.devices
    readonly property var direct: devices.filter(d => d.route === "direct" || d.route === "unknown")
    readonly property var relayed: devices.filter(d => d.route === "relay")
    readonly property var connecting: devices.filter(d => d.route === "connecting")
    readonly property bool twoColumns: direct.length > 0 && relayed.length > 0
    readonly property real methodWidth: wide ? 168
        : (width - 2 * margin - (Math.max(methods.length, 1) - 1) * 8) / Math.max(methods.length, 1)
    readonly property real busX: margin + 168 + 64         // wide
    readonly property real busY: methodsBottom + 40       // tall
    readonly property real methodsBottom: wide ? 64 + methods.length * 56 - 12 : 88 + 60
    readonly property real middleY: wide ? (64 + methodsBottom) / 2 : 0
    readonly property real groupsTop: wide ? 64 : device.y + device.height + 76
    readonly property real groupColumn: (width - 2 * margin - 12) / 2
    readonly property real wideGroupsX: busX + 76 + 180 + 72 // right of this device
    readonly property real groupWidth: wide ? width - wideGroupsX - margin : groupColumn
    readonly property real centeredX: (width - groupWidth) / 2
    readonly property alias deviceCard: device
    readonly property alias directCard: directGroup
    readonly property alias relayCard: relayGroup

    function methodCenter(index) {
        return wide ? Qt.point(margin + 168, 64 + index * 56 + 22)
                    : Qt.point(margin + index * (methodWidth + 8) + methodWidth / 2, methodsBottom);
    }

    implicitHeight: legend.y + legend.height + 24
    onWidthChanged: lines.requestPaint()
    onDevicesChanged: lines.requestPaint()
    onMethodsChanged: lines.requestPaint()

    MapLines {
        id: lines

        anchors.fill: parent
        map: map
    }

    Label {
        x: map.margin
        y: 20
        text: qsTr("Right now")
        font.family: Theme.headingFont
        font.pixelSize: 17
        font.weight: Font.DemiBold
        color: Theme.ink
    }

    Label {
        x: map.wide ? map.margin + 110 : map.margin
        y: map.wide ? 23 : 46
        width: map.width - x - map.margin
        elide: Text.ElideRight
        font.pixelSize: 13
        color: Theme.muted
        text: map.devices.length === 0 ? qsTr("No other devices connected yet")
            : qsTr("%1 connected · %2 direct, %3 through a relay")
                  .arg(map.direct.length + map.relayed.length).arg(map.direct.length).arg(map.relayed.length)
              + (map.connecting.length > 0 ? qsTr(" · %1 connecting").arg(map.connecting.length) : "")
    }

    Repeater {
        model: map.methods

        MapMethodCard {
            required property var modelData
            required property int index

            method: modelData
            x: map.wide ? map.margin : map.margin + index * (map.methodWidth + 8)
            y: map.wide ? 64 + index * 56 : 88
            width: map.methodWidth
            height: map.wide ? 44 : 60
        }
    }

    Rectangle {
        id: device

        x: map.wide ? map.busX + 76 : (map.width - width) / 2
        y: map.wide ? map.middleY - height / 2 : map.busY + 40
        width: 180
        height: 64
        radius: 12
        color: Theme.accentSoft
        border.width: 1.5
        border.color: Theme.accent

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Icon {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                path: Icons.laptop
                color: Theme.accent
            }

            ColumnLayout {
                spacing: 0

                Label {
                    text: qsTr("This device")
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.ink
                }

                Label {
                    text: Network.online ? qsTr("You · online") : qsTr("You · offline")
                    font.pixelSize: 12
                    color: Theme.accentInk
                }
            }
        }
    }

    MapDeviceGroup {
        id: directGroup

        visible: map.direct.length > 0
        route: "direct"
        devices: map.direct
        avatarsOnly: map.wide
        x: map.wide ? map.wideGroupsX : map.twoColumns ? map.margin : map.centeredX
        y: map.groupsTop
        width: map.groupWidth
    }

    MapDeviceGroup {
        id: relayGroup

        visible: map.relayed.length > 0
        route: "relay"
        devices: map.relayed
        avatarsOnly: map.wide
        x: map.wide ? map.wideGroupsX : map.twoColumns ? map.margin + map.groupColumn + 12 : map.centeredX
        y: map.wide && directGroup.visible ? directGroup.y + directGroup.height + 16 : map.groupsTop
        width: map.groupWidth
    }

    MapDeviceGroup {
        id: connectingGroup

        visible: map.connecting.length > 0
        route: "connecting"
        devices: map.connecting
        avatarsOnly: map.wide
        x: map.wide || !directGroup.visible ? relayGroup.x : map.margin + map.groupColumn + 12
        y: relayGroup.visible ? relayGroup.y + relayGroup.height + 16
         : map.wide && directGroup.visible ? directGroup.y + directGroup.height + 16 : map.groupsTop
        width: map.groupWidth
    }

    MapLegend {
        id: legend

        x: map.margin
        y: Math.max(map.methodsBottom, device.y + device.height,
                    directGroup.visible ? directGroup.y + directGroup.height : 0,
                    relayGroup.visible ? relayGroup.y + relayGroup.height : 0,
                    connectingGroup.visible ? connectingGroup.y + connectingGroup.height : 0) + 24
        width: map.width - 2 * map.margin
    }
}
