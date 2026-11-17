pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The start screen: pick or create a project, then pick or create one of its boards.
Item {
    id: home

    readonly property bool hasProject: Library.currentProjectId.length > 0

    signal openBoard(string boardId)
    signal openSettings()

    function createBoard(name: string) {
        const id = Library.createBoard(name);
        if (id.length > 0)
            home.openBoard(id);
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ProjectSidebar {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            onCreateProjectRequested: newProjectDialog.ask("")
            onJoinProjectRequested: joinDialog.ask("")
            onSettingsRequested: home.openSettings()
            onRequestsRequested: requestsDialog.open()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            WelcomePanel {
                anchors.centerIn: parent
                visible: !home.hasProject
                onCreateProjectRequested: newProjectDialog.ask("")
                onJoinProjectRequested: joinDialog.ask("")
                onLinkDeviceRequested: linkDialog.ask("")
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 32
                spacing: 20
                visible: home.hasProject

                ProjectHeader {
                    Layout.fillWidth: true
                    onMembersRequested: membersDialog.show(Library.currentProjectId, false)
                    onRequestsRequested: requestsDialog.open()
                    onNewBoardRequested: newBoardDialog.ask(qsTr("Untitled board"))
                    onRenameRequested: renameProjectDialog.ask(Library.currentProjectName)
                    onDeleteRequested: deleteProjectDialog.open()
                }

                TextField {
                    Layout.preferredWidth: 320
                    placeholderText: qsTr("Search boards")
                    Accessible.name: qsTr("Search boards")
                    text: Library.boardFilter
                    onTextEdited: Library.boardFilter = text
                }

                GridView {
                    id: grid

                    readonly property int columns: Math.max(1, Math.floor(width / 260))

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    cellWidth: width / columns
                    cellHeight: 240
                    model: Library.boards

                    delegate: Item {
                        id: cell

                        required property string boardId
                        required property string name
                        required property date edited
                        required property bool admin
                        required property bool pending

                        width: grid.cellWidth
                        height: grid.cellHeight

                        BoardCard {
                            anchors.fill: parent
                            anchors.margins: 9
                            boardId: cell.boardId
                            admin: cell.admin
                            pending: cell.pending
                            name: cell.name
                            edited: cell.edited
                            onClicked: {
                                if (!cell.pending)
                                    home.openBoard(cell.boardId);
                            }
                            onRenameRequested: (boardId, name) => {
                                renameBoardDialog.boardId = boardId;
                                renameBoardDialog.ask(name);
                            }
                            onShareRequested: boardId => membersDialog.show(boardId, true)
                            onDeleteRequested: (boardId, name) => {
                                const admin = Library.isBoardAdmin(boardId);
                                deleteBoardDialog.boardId = boardId;
                                deleteBoardDialog.confirmText = admin ? qsTr("Delete") : qsTr("Remove");
                                deleteBoardDialog.message = admin
                                    ? qsTr("Delete the board “%1” for everyone? Its drawings are removed and cannot be recovered.").arg(name)
                                    : qsTr("Remove the board “%1” from this device? You can join again with an invite.").arg(name);
                                deleteBoardDialog.open();
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: grid.count === 0
                        width: Math.min(parent.width, 360)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        text: Library.boardFilter.length > 0
                            ? qsTr("No boards match your search.")
                            : qsTr("This project has no boards yet. Create one with “New board”.")
                    }
                }
            }
        }
    }

    TextInputDialog {
        id: newProjectDialog

        title: qsTr("New project")
        placeholder: qsTr("Project name")
        acceptText: qsTr("Create")
        onSubmitted: text => Library.createProject(text)
    }

    TextInputDialog {
        id: joinDialog

        title: qsTr("Join a project or board")
        label: qsTr("Paste the invite code its admin sent you. The admin must also have added you as a member.")
        placeholder: "peerchalk-invite:2:…"
        acceptText: qsTr("Join")
        onSubmitted: text => Library.joinInvite(text)
    }

    TextInputDialog {
        id: renameProjectDialog

        title: qsTr("Rename project")
        onSubmitted: text => Library.renameProject(Library.currentProjectId, text)
    }

    ConfirmDialog {
        id: deleteProjectDialog

        title: qsTr("Delete project")
        confirmText: Library.currentProjectIsAdmin ? qsTr("Delete") : qsTr("Leave")
        message: Library.currentProjectIsAdmin
            ? qsTr("Delete “%1” for everyone? Members lose access to it. Boards you made stay with you.").arg(Library.currentProjectName)
            : qsTr("Leave “%1”? It is removed from this device; you can join again with an invite.").arg(Library.currentProjectName)
        onConfirmed: Library.removeProject(Library.currentProjectId)
    }

    TextInputDialog {
        id: newBoardDialog

        title: qsTr("New infinite board")
        placeholder: qsTr("Board name")
        acceptText: qsTr("Create")
        onSubmitted: text => home.createBoard(text)
    }

    TextInputDialog {
        id: renameBoardDialog

        property string boardId

        title: qsTr("Rename board")
        onSubmitted: text => Library.renameBoard(boardId, text)
    }

    ConfirmDialog {
        id: deleteBoardDialog

        property string boardId

        title: qsTr("Delete board")
        onConfirmed: Library.removeBoard(boardId)
    }

    MembersDialog {
        id: membersDialog
    }

    RequestsDialog {
        id: requestsDialog
    }

    TextInputDialog {
        id: linkDialog

        title: qsTr("Link this device")
        label: qsTr("On a device where you already use PeerChalk, open Settings → People, choose “Link another device” and paste its code here. This device then acts as you and gets your projects once both are online.")
        placeholder: "peerchalk-identity:1:…"
        acceptText: qsTr("Link")
        onSubmitted: text => Library.adoptIdentity(text)
    }
}
