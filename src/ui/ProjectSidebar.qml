pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The left column of the home screen: the way back to the accounts, the app name with any
// update, the projects, and you (opening the settings).
Rectangle {
    id: sidebar

    signal createProjectRequested()
    signal joinProjectRequested()
    signal settingsRequested()
    signal requestsRequested()

    color: Theme.surface
    implicitWidth: 280

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ToolButton {
                Layout.leftMargin: -6
                implicitWidth: 32
                implicitHeight: 32
                Accessible.name: qsTr("Back to the accounts")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                ToolTip.delay: 400
                onClicked: Accounts.signOut()

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        path: Icons.chevronLeft
                        color: Theme.ink
                        lineWidth: 2
                    }
                }
            }

            Image {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                source: "qrc:/PeerChalk/peerchalk-logo.png"
                sourceSize: Qt.size(128, 128)
                fillMode: Image.PreserveAspectFit
                mipmap: true
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("PeerChalk icon")
            }

            Label {
                text: qsTr("PeerChalk")
                font.family: Theme.headingFont
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            VersionLabel {
                Layout.alignment: Qt.AlignBaseline
                Layout.leftMargin: -4
                objectName: "appVersion"
            }

            Item {
                Layout.fillWidth: true
            }

            UpdateBadge {}
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            PaperButton {
                Layout.fillWidth: true
                text: qsTr("New project")
                primary: true
                onClicked: sidebar.createProjectRequested()
            }

            PaperButton {
                text: qsTr("Join")
                onClicked: sidebar.joinProjectRequested()
            }
        }

        SectionLabel {
            Layout.leftMargin: 10
            text: qsTr("Projects")
            visible: Library.projects.count > 0
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: Library.projects

            delegate: ItemDelegate {
                id: row

                required property string projectId
                required property string name
                required property int colorIndex
                required property int boardCount
                required property bool pending
                readonly property bool current: Library.currentProjectId === projectId

                width: ListView.view.width
                height: 44
                onClicked: Library.currentProjectId = projectId

                background: Rectangle {
                    radius: Theme.controlRadius
                    color: row.current ? Theme.accentSoft : row.hovered ? Theme.hover : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 10

                    Rectangle {
                        implicitWidth: 10
                        implicitHeight: 10
                        radius: 3
                        color: Theme.projectColors[row.colorIndex % Theme.projectColors.length]
                    }

                    Label {
                        Layout.fillWidth: true
                        text: row.name
                        elide: Text.ElideRight
                        font.weight: row.current ? Font.DemiBold : Font.Normal
                        color: Theme.ink
                    }

                    Label {
                        text: row.pending ? qsTr("waiting") : row.boardCount
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }
            }
        }

        ItemDelegate {
            id: sharedRow

            readonly property bool current: Library.currentProjectId === Library.sharedId

            Layout.fillWidth: true
            visible: Library.sharedBoardCount > 0 || current
            Layout.preferredHeight: 44
            onClicked: Library.currentProjectId = Library.sharedId

            background: Rectangle {
                radius: Theme.controlRadius
                color: sharedRow.current ? Theme.accentSoft : sharedRow.hovered ? Theme.hover : "transparent"
            }

            contentItem: RowLayout {
                spacing: 10

                Icon {
                    implicitWidth: 14
                    implicitHeight: 14
                    path: Icons.share
                    color: Theme.muted
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Shared with me")
                    font.weight: sharedRow.current ? Font.DemiBold : Font.Normal
                    color: Theme.ink
                }

                Label {
                    text: Library.sharedBoardCount
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }
        }

        PaperButton {
            Layout.fillWidth: true
            visible: Network.joinRequestCount > 0
            primary: true
            text: Network.joinRequestCount === 1 ? qsTr("1 request to join") : qsTr("%1 requests to join").arg(Network.joinRequestCount)
            onClicked: sidebar.requestsRequested()
        }

        NetworkStatus {
            Layout.fillWidth: true
            Layout.leftMargin: 10
        }

        ItemDelegate {
            id: settingsButton

            Layout.fillWidth: true
            Accessible.name: qsTr("Settings")
            onClicked: sidebar.settingsRequested()

            background: Rectangle {
                radius: 12
                color: settingsButton.hovered ? Theme.hover : "transparent"
                border.color: Theme.border
            }

            contentItem: RowLayout {
                spacing: 10

                Avatar {
                    name: Library.displayName
                    size: 34
                    color: Theme.personColors[Library.colorIndex]
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: Library.displayName
                        elide: Text.ElideRight
                        font.weight: Font.DemiBold
                        color: Theme.ink
                    }

                    Label {
                        text: qsTr("Settings")
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }
            }
        }
    }
}
