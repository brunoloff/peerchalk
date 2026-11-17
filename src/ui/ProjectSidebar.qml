pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The left column of the home screen: the app name, the projects and the identity.
Rectangle {
    id: sidebar

    signal createProjectRequested()
    signal joinProjectRequested()
    signal identityRequested()

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
            spacing: 10

            Rectangle {
                implicitWidth: 28
                implicitHeight: 28
                radius: 7
                color: Theme.ink

                Icon {
                    anchors.centerIn: parent
                    path: "M5 15c3-6 6 2 9-4s3-3 5-2"
                    color: Theme.background
                    lineWidth: 2.4
                }
            }

            Label {
                text: qsTr("PeerChalk")
                font.family: Theme.headingFont
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: Theme.ink
            }
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
                        text: row.pending ? qsTr("joining") : row.boardCount
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

        ItemDelegate {
            id: identityButton

            Layout.fillWidth: true
            Accessible.name: qsTr("Identity and contacts")
            onClicked: sidebar.identityRequested()

            background: Rectangle {
                radius: 12
                color: identityButton.hovered ? Theme.hover : "transparent"
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
                        text: qsTr("Identity & contacts")
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }
            }
        }
    }
}
