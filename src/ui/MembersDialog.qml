pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Who has which role on a project or a board, and the controls your own role allows:
// the admin changes anyone's role and can hand the admin role over; owners add and
// remove members. For a board, owners also choose which of their projects it is in.
Dialog {
    id: dialog

    property string itemId
    property bool board: false
    property string myRole: "none"
    readonly property bool canManage: myRole === "owner" || myRole === "admin"
    property var people: []
    property var projects: []

    function show(id: string, isBoard: bool) {
        itemId = id;
        board = isBoard;
        refresh();
        open();
    }

    function refresh() {
        myRole = board ? Library.boardRole(itemId) : Library.projectRole(itemId);
        people = board ? Library.boardPeople(itemId) : Library.projectPeople(itemId);
        projects = board ? Library.boardProjects(itemId) : [];
        linkSection.refresh();
    }

    function setRole(key: string, role: string) {
        if (board)
            Library.setBoardRole(itemId, key, role);
        else
            Library.setProjectRole(itemId, key, role);
        refresh();
    }

    function makeAdmin(key: string) {
        if (board)
            Library.transferBoardAdmin(itemId, key);
        else
            Library.transferProjectAdmin(itemId, key);
        refresh();
    }

    title: board ? qsTr("Share “%1”").arg(Library.boardName(itemId)) : qsTr("People in “%1”").arg(Library.currentProjectName)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 32)
    modal: true

    Connections {
        target: Library

        function onWorkspaceChanged() {
            if (dialog.opened)
                dialog.refresh();
        }
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.muted
                text: dialog.myRole === "admin" ? qsTr("You are the admin: you decide who are owners and members, and can hand the admin role to someone else.")
                    : dialog.myRole === "owner" ? qsTr("You are an owner: you can add and remove members.")
                    : qsTr("Only owners and the admin change who has access.")
            }

            Repeater {
                model: dialog.people

                delegate: PersonRoleRow {
                    myRole: dialog.myRole
                    onRoleRequested: (key, role) => dialog.setRole(key, role)
                    onAdminRequested: key => dialog.makeAdmin(key)
                }
            }

            SectionLabel {
                visible: dialog.projects.length > 0
                text: qsTr("In projects")
            }

            Repeater {
                model: dialog.projects

                delegate: CheckBox {
                    id: projectBox

                    required property var modelData

                    text: projectBox.modelData.name
                    checked: projectBox.modelData.included
                    onToggled: {
                        Library.setBoardInProject(dialog.itemId, projectBox.modelData.projectId, checked);
                        dialog.refresh();
                    }
                }
            }

            BoardLinkSection {
                id: linkSection

                Layout.fillWidth: true
                visible: dialog.board && dialog.canManage
                boardId: dialog.board ? dialog.itemId : ""
            }

            RowLayout {
                Layout.fillWidth: true
                visible: dialog.canManage
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    text: qsTr("Send the invite privately. When someone opens it, they appear under Requests for you or another owner to approve.")
                }

                PaperButton {
                    text: qsTr("Copy invite")
                    primary: true
                    onClicked: Library.copyToClipboard(dialog.board ? Library.boardInvite(dialog.itemId) : Library.projectInvite(dialog.itemId))
                }
            }
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
