pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Everyone asking to join a project or board you own, with an Approve button each.
Dialog {
    id: dialog

    property var requests: []

    function refresh() {
        requests = Network.allJoinRequests();
    }

    title: qsTr("Requests to join")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 32)
    modal: true
    onAboutToShow: refresh()

    Connections {
        target: Network

        function onRequestsChanged() {
            if (dialog.opened)
                dialog.refresh();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            text: dialog.requests.length === 0
                ? qsTr("No one is asking to join right now. People appear here while they are online with an invite you or another owner sent.")
                : qsTr("Anyone with the invite can ask. To be sure it is really them, compare the fingerprint with the one in their Settings → People.")
        }

        Repeater {
            model: dialog.requests

            delegate: RowLayout {
                id: request

                required property var modelData

                Layout.fillWidth: true
                spacing: 12

                Avatar {
                    name: request.modelData.name
                    size: 34
                    color: Theme.personColors[Format.keyColorIndex(request.modelData.key, Theme.personColors.length)]
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.weight: Font.DemiBold
                        color: Theme.ink
                        text: qsTr("%1 → %2 “%3”").arg(request.modelData.name)
                            .arg(request.modelData.board ? qsTr("board") : qsTr("project"))
                            .arg(request.modelData.itemName)
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                        text: request.modelData.fingerprint
                        font.family: Theme.monoFont
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }

                PaperButton {
                    text: qsTr("Approve")
                    primary: true
                    onClicked: {
                        if (request.modelData.board)
                            Library.admitToBoard(request.modelData.itemId, request.modelData.key, request.modelData.name);
                        else
                            Library.admitToProject(request.modelData.itemId, request.modelData.key, request.modelData.name);
                        dialog.refresh();
                    }
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
