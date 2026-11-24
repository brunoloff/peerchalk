pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The devices reached one way on the connection map: directly, through a relay, or still
// connecting. Shows names in rows, the first few unless expanded, or only avatars.
Rectangle {
    id: group

    required property string route // "direct", "relay" or "connecting"
    required property var devices  // from Network.devices
    property bool avatarsOnly: false
    property int maxRows: 6
    property bool expanded: false
    readonly property var shown: expanded ? devices : devices.slice(0, maxRows)
    readonly property color ink: route === "direct" ? Theme.accent
                               : route === "relay" ? Theme.relayInk : Theme.quiet

    function nameOf(device) {
        return device.name.length > 0 ? device.name : qsTr("Unknown device");
    }

    implicitHeight: body.implicitHeight + 2
    radius: 12
    color: route === "connecting" ? "#FAF9F6" : Theme.surface
    border.color: route === "direct" ? Theme.directBorder
                : route === "relay" ? Theme.relayBorder : "#CFCAC0"
    clip: true

    ColumnLayout {
        id: body

        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            radius: 11
            color: group.route === "direct" ? Theme.directSoft
                 : group.route === "relay" ? Theme.relaySoft : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Icon {
                    visible: group.route === "relay"
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    path: Icons.turn
                    color: group.ink
                }

                Label {
                    Layout.fillWidth: true
                    font.family: Theme.headingFont
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.9
                    color: group.ink
                    text: (group.route === "direct" ? qsTr("DIRECT")
                         : group.route === "relay" ? qsTr("VIA RELAY") : qsTr("CONNECTING"))
                          + " · " + group.devices.length
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.margins: 10
            visible: group.avatarsOnly
            spacing: 4

            Repeater {
                model: group.avatarsOnly ? group.devices : []

                MapAvatar {
                    required property var modelData

                    device: modelData
                    size: 26
                    ToolTip.text: group.nameOf(modelData)
                    ToolTip.visible: hovered
                }
            }
        }

        Repeater {
            model: group.avatarsOnly ? [] : group.shown

            RowLayout {
                id: deviceRow

                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 8

                MapAvatar {
                    device: deviceRow.modelData
                    size: 20
                }

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    color: Theme.ink
                    text: group.nameOf(deviceRow.modelData)
                }

                Label {
                    font.pixelSize: 12
                    color: Theme.quiet
                    text: deviceRow.modelData.route === "unknown" ? qsTr("route unknown")
                        : group.route === "connecting" ? qsTr("finding a route…") : ""
                }
            }
        }

        Button {
            id: showAll

            objectName: "showAllDevices"
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: !group.avatarsOnly && group.devices.length > group.maxRows
            flat: true
            text: group.expanded ? qsTr("Show fewer")
                                 : qsTr("+%1 more · Show all").arg(group.devices.length - group.maxRows)
            font.pixelSize: 13
            onClicked: group.expanded = !group.expanded

            contentItem: Label {
                text: showAll.text
                font: showAll.font
                color: Theme.accent
                leftPadding: 4
            }

            background: Item {}
        }

        Item {
            Layout.preferredHeight: 4
        }
    }
}
