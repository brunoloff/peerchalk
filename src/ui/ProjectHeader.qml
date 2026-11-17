import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The title row of the home screen for the selected project (or "Shared with me").
RowLayout {
    id: header

    readonly property bool isShared: Library.currentProjectId === Library.sharedId
    readonly property int memberCount: {
        Library.workspaceChanged; // re-evaluate when members change
        return isShared ? 0 : Library.projectPeople(Library.currentProjectId).filter(p => p.member).length;
    }

    signal membersRequested()
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
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.muted
            text: header.isShared ? qsTr("Boards shared with you on their own, and boards you made outside projects")
                : Library.currentProjectPending ? qsTr("Joining · waiting to reach someone already in this project")
                : header.memberCount <= 1 ? qsTr("Only you so far")
                : qsTr("%1 people").arg(header.memberCount)
        }
    }

    RailButton {
        visible: !header.isShared
        text: qsTr("Project options")
        iconPath: Icons.more
        onClicked: menu.popup()

        Menu {
            id: menu

            MenuItem {
                text: qsTr("Rename…")
                enabled: Library.currentProjectIsAdmin
                onTriggered: header.renameRequested()
            }

            MenuItem {
                text: Library.currentProjectIsAdmin ? qsTr("Delete for everyone…") : qsTr("Leave project…")
                onTriggered: header.deleteRequested()
            }
        }
    }

    PaperButton {
        visible: !header.isShared && !Library.currentProjectPending
        text: Library.currentProjectIsAdmin ? qsTr("Members & invite") : qsTr("Members")
        onClicked: header.membersRequested()
    }

    PaperButton {
        visible: !Library.currentProjectPending
        text: qsTr("New board")
        strong: true
        onClicked: header.newBoardRequested()
    }
}
