pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The start screen: pick or create a project, then pick or create one of its boards.
Item {
    id: home

    readonly property bool hasProject: Library.currentProjectId.length > 0

    signal openBoard(string boardId)
    signal openIdentity()

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
            onIdentityRequested: home.openIdentity()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            WelcomePanel {
                anchors.centerIn: parent
                visible: !home.hasProject
                onCreateProjectRequested: newProjectDialog.ask("")
                onJoinProjectRequested: joinDialog.ask("")
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 32
                spacing: 20
                visible: home.hasProject

                ProjectHeader {
                    Layout.fillWidth: true
                    onInviteRequested: {
                        inviteDialog.code = Library.projectInvite(Library.currentProjectId);
                        inviteDialog.open();
                    }
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

                        width: grid.cellWidth
                        height: grid.cellHeight

                        BoardCard {
                            anchors.fill: parent
                            anchors.margins: 9
                            boardId: cell.boardId
                            name: cell.name
                            edited: cell.edited
                            onClicked: home.openBoard(cell.boardId)
                            onRenameRequested: (boardId, name) => {
                                renameBoardDialog.boardId = boardId;
                                renameBoardDialog.ask(name);
                            }
                            onDeleteRequested: (boardId, name) => {
                                deleteBoardDialog.boardId = boardId;
                                deleteBoardDialog.message = qsTr("Delete the board “%1”? Its drawings on this device are removed and cannot be recovered.").arg(name);
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

        title: qsTr("Join a project")
        label: qsTr("Paste the invite code someone sent you.")
        placeholder: "peerchalk-invite:1:…"
        acceptText: qsTr("Join")
        onSubmitted: text => Library.joinProject(text)
    }

    TextInputDialog {
        id: renameProjectDialog

        title: qsTr("Rename project")
        onSubmitted: text => Library.renameProject(Library.currentProjectId, text)
    }

    ConfirmDialog {
        id: deleteProjectDialog

        title: qsTr("Delete project")
        message: qsTr("Delete “%1” and all its boards from this device? This cannot be undone.").arg(Library.currentProjectName)
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

    CodeDialog {
        id: inviteDialog

        title: qsTr("Invite to %1").arg(Library.currentProjectName)
        explanation: qsTr("Send this code privately to the people you want in this project. Anyone who has it can join, so don't post it publicly.")
    }
}
