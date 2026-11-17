import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings: your identity and known people, and the network.
Item {
    id: page

    property alias currentTab: tabs.currentIndex

    signal back()

    objectName: "settingsPage"

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("Settings")
            onBack: page.back()
        }

        TabBar {
            id: tabs

            Layout.fillWidth: true
            leftPadding: 24

            background: Rectangle {
                color: Theme.surface

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.border
                }
            }

            TabButton {
                width: implicitWidth + 24
                text: qsTr("People")
            }

            TabButton {
                width: implicitWidth + 24
                text: qsTr("Network")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            PeoplePane {}

            NetworkPane {
                objectName: "networkPane"
            }
        }
    }
}
