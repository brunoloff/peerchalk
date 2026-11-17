import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The title row of the home screen for the selected project.
RowLayout {
    id: header

    signal inviteRequested()
    signal newBoardRequested()
    signal renameRequested()
    signal deleteRequested()

    spacing: 12

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        Label {
            Layout.fillWidth: true
            text: Library.currentProjectName
            elide: Text.ElideRight
            font.family: Theme.headingFont
            font.pixelSize: 30
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        Label {
            text: qsTr("Only you so far · invite people to draw together")
            color: Theme.muted
        }
    }

    RailButton {
        text: qsTr("Project options")
        iconPath: Icons.more
        onClicked: menu.popup()

        Menu {
            id: menu

            MenuItem {
                text: qsTr("Rename…")
                onTriggered: header.renameRequested()
            }

            MenuItem {
                text: qsTr("Delete…")
                onTriggered: header.deleteRequested()
            }
        }
    }

    PaperButton {
        text: qsTr("Invite")
        onClicked: header.inviteRequested()
    }

    PaperButton {
        text: qsTr("New board")
        strong: true
        onClicked: header.newBoardRequested()
    }
}
