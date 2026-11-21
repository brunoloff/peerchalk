import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The title row of the home screen for the selected project (or "Shared with me").
RowLayout {
    id: header

    readonly property bool isShared: Library.currentProjectId === Library.sharedId
    readonly property int memberCount: {
        Library.workspaceRevision; // re-evaluate when members change
        return isShared ? 0 : Library.projectMemberCount(Library.currentProjectId);
    }

    readonly property string myRole: {
        Library.workspaceRevision; // re-evaluate when roles change
        return isShared ? "none" : Library.projectRole(Library.currentProjectId);
    }
    readonly property bool canManage: myRole === "owner" || myRole === "admin"
    readonly property int requestCount: {
        Network.joinRequestCount; // re-evaluate when requests change
        return canManage ? Network.joinRequests(Library.currentProjectId).length : 0;
    }

    signal membersRequested()
    signal requestsRequested()
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
            objectName: "projectSubtitle"
            text: header.isShared ? qsTr("Boards shared with you on their own, and boards you made outside projects")
                : Library.currentProjectPending ? qsTr("Waiting for the admin to let you in · keep PeerChalk open")
                : header.memberCount <= 1 ? qsTr("Only you so far")
                : qsTr("%1 people").arg(header.memberCount)
        }
    }

    RailButton {
        visible: !header.isShared
        text: qsTr("Project options")
        iconPath: Icons.more
        onClicked: menu.popup()

        PaperMenu {
            id: menu

            PaperMenuItem {
                text: qsTr("Rename…")
                enabled: Library.currentProjectIsAdmin
                onTriggered: header.renameRequested()
            }

            PaperMenuItem {
                text: Library.currentProjectIsAdmin ? qsTr("Delete for everyone…") : qsTr("Leave project…")
                destructive: true
                onTriggered: header.deleteRequested()
            }
        }
    }

    PaperButton {
        visible: header.requestCount > 0
        primary: true
        text: header.requestCount === 1 ? qsTr("1 asking to join") : qsTr("%1 asking to join").arg(header.requestCount)
        onClicked: header.requestsRequested()
    }

    PaperButton {
        visible: !header.isShared && !Library.currentProjectPending
        text: header.canManage ? qsTr("People & invite") : qsTr("People")
        onClicked: header.membersRequested()
    }

    PaperButton {
        visible: !Library.currentProjectPending && (header.isShared || header.canManage)
        text: qsTr("New board")
        strong: true
        onClicked: header.newBoardRequested()
    }
}
