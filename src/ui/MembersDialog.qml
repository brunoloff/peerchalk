pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Who may access a project or a board. The admin ticks people from their known people
// and sends them the invite code; others see the list. For a board, the admin also
// chooses which of their projects it belongs to.
Dialog {
    id: dialog

    property string itemId
    property bool board: false
    readonly property bool admin: board ? Library.isBoardAdmin(itemId) : Library.isProjectAdmin(itemId)
    property var people: []
    property var projects: []

    function show(id: string, isBoard: bool) {
        itemId = id;
        board = isBoard;
        refresh();
        open();
    }

    function refresh() {
        people = board ? Library.boardPeople(itemId) : Library.projectPeople(itemId);
        projects = board && admin ? Library.boardProjects(itemId) : [];
    }

    function invite(): string {
        return board ? Library.boardInvite(itemId) : Library.projectInvite(itemId);
    }

    title: board ? qsTr("Share “%1”").arg(Library.boardName(itemId)) : qsTr("Members of “%1”").arg(Library.currentProjectName)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 32)
    modal: true

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
                text: !dialog.admin ? qsTr("Only the admin decides who has access.")
                    : dialog.board ? qsTr("Tick the people who may open this board on its own, then send them the invite. Members of the projects it belongs to can open it too.")
                    : qsTr("Tick the people who may open every board in this project, then send them the invite.")
            }

            Repeater {
                model: dialog.people

                delegate: RowLayout {
                    id: row

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 12

                    Avatar {
                        name: row.modelData.name
                        size: 34
                        color: Theme.personColors[Format.keyColorIndex(row.modelData.key, Theme.personColors.length)]
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: Theme.ink
                            font.weight: Font.DemiBold
                            text: row.modelData.name + (row.modelData.you ? qsTr(" (you)") : "")
                                + (row.modelData.admin ? qsTr(" · admin") : "")
                        }

                        Label {
                            text: row.modelData.fingerprint.slice(0, 19) + " …"
                            font.family: Theme.monoFont
                            font.pixelSize: 12
                            color: Theme.muted
                        }
                    }

                    CheckBox {
                        checked: row.modelData.member
                        enabled: dialog.admin && !row.modelData.admin
                        Accessible.name: qsTr("%1 has access").arg(row.modelData.name)
                        onToggled: {
                            if (dialog.board)
                                Library.setBoardMember(dialog.itemId, row.modelData.key, checked);
                            else
                                Library.setProjectMember(dialog.itemId, row.modelData.key, checked);
                            dialog.refresh();
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: dialog.admin && Library.contacts.count === 0
                wrapMode: Text.WordWrap
                color: Theme.muted
                text: qsTr("To give someone access, first add them to your known people on the identity screen, using their contact code.")
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

            RowLayout {
                Layout.fillWidth: true
                visible: dialog.admin
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    text: qsTr("The invite lets the people you ticked find this %1. Send it privately.").arg(dialog.board ? qsTr("board") : qsTr("project"))
                }

                PaperButton {
                    text: qsTr("Copy invite")
                    primary: true
                    onClicked: Library.copyToClipboard(dialog.invite())
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
