pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The board's network pane, opened from the network button beside its title: whether this
// device reaches the others on the board, why not when it does not and what to try, who is
// connected and how, the servers it finds them through, and the log.
Rectangle {
    id: pane

    // Network.boardConnection for the open board, from the network button.
    required property var status
    readonly property string connectionState: status.state
    readonly property var facts: status.facts
    readonly property var tips: ConnectionAdvice.tips(connectionState, facts)

    signal closeRequested()

    color: Theme.surface
    implicitWidth: 340

    // The servers are checked again while the pane is open, so that what it says is current.
    Timer {
        interval: 60 * 1000
        running: pane.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: Network.checkServers()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            x: 20
            width: scroll.availableWidth - 40
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 16

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Connection")
                    font.family: Theme.headingFont
                    font.pixelSize: 20
                    color: Theme.ink
                }

                RailButton {
                    implicitWidth: 32
                    implicitHeight: 32
                    text: qsTr("Close the connection pane")
                    iconPath: Icons.close
                    onClicked: pane.closeRequested()
                }
            }

            Rectangle {
                objectName: "connectionVerdict"
                Layout.fillWidth: true
                implicitHeight: verdict.implicitHeight + 28
                radius: Theme.controlRadius
                color: ConnectionAdvice.tone(pane.connectionState) === "problem" ? Theme.warningSoft : Theme.background
                border.color: Theme.border

                ColumnLayout {
                    id: verdict

                    x: 14
                    y: 14
                    width: parent.width - 28
                    spacing: 6

                    RowLayout {
                        spacing: 8

                        Rectangle {
                            implicitWidth: 10
                            implicitHeight: 10
                            radius: 5
                            color: ConnectionAdvice.color(pane.connectionState)
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: ConnectionAdvice.title(pane.connectionState, pane.facts)
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: Theme.ink
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: ConnectionAdvice.detail(pane.connectionState, pane.facts)
                        font.pixelSize: 13
                        color: Theme.muted
                    }

                    SectionLabel {
                        Layout.topMargin: 6
                        visible: pane.tips.length > 0
                        text: qsTr("What you can try")
                    }

                    Repeater {
                        model: pane.tips

                        Label {
                            required property string modelData
                            required property int index

                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: (index + 1) + ". " + modelData
                            font.pixelSize: 13
                            color: Theme.ink
                        }
                    }
                }
            }

            SectionLabel {
                text: qsTr("People on this board")
            }

            BoardNetworkPeople {
                Layout.fillWidth: true
                people: pane.status.people
            }

            SectionLabel {
                visible: pane.status.repository.open
                text: qsTr("Repository")
            }

            Label {
                Layout.fillWidth: true
                visible: pane.status.repository.open
                wrapMode: Text.Wrap
                text: pane.status.repository.open ? ConnectionAdvice.repositoryLine(pane.status.repository) : ""
                font.pixelSize: 13
                color: Theme.ink
            }

            SectionLabel {
                text: qsTr("How PeerChalk finds the others")
            }

            ConnectionChips {
                Layout.fillWidth: true
                enabled: false // the settings are elsewhere
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: Theme.muted
                text: qsTr("Discovery servers: %1 answering, %2 not, %3 being checked. Relay: %4. This board is %5.")
                    .arg(pane.facts.discoveryWorking ?? 0).arg(pane.facts.discoveryUnreachable ?? 0)
                    .arg(pane.facts.discoveryChecking ?? 0)
                    .arg(!pane.facts.relayConfigured ? qsTr("none set up")
                         : pane.facts.relayWorking ? qsTr("answering") : qsTr("not answering"))
                    .arg(pane.facts.announced ? qsTr("announced") : qsTr("not announced yet"))
            }

            SectionLabel {
                text: qsTr("Log")
            }

            ConnectionLogSection {
                Layout.fillWidth: true
                Layout.bottomMargin: 20
            }
        }
    }
}
