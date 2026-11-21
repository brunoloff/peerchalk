import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One board on the home screen: a live preview, its name and when it was last edited.
// Used as a GridView delegate over Library.boards.
AbstractButton {
    id: card

    required property string boardId
    required property string name
    required property date edited

    required property bool admin // you own the board
    required property bool pending

    signal renameRequested(string boardId, string name)
    signal shareRequested(string boardId)
    signal deleteRequested(string boardId, string name)

    hoverEnabled: true
    Accessible.name: qsTr("Open board %1").arg(card.name)

    background: Rectangle {
        radius: Theme.panelRadius
        color: Theme.surface
        border.color: card.hovered ? Theme.accent : Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 144
            Layout.margins: 6
            radius: Theme.controlRadius
            color: Theme.boardBackground
            clip: true

            BoardPreview {
                id: preview

                anchors.fill: parent
                // Depending on `edited` reloads the preview whenever the board is saved.
                boardState: card.edited.getTime() >= 0 ? Library.boardState(card.boardId) : ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: card.name
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    color: Theme.ink
                }

                Label {
                    Layout.fillWidth: true
                    text: card.pending ? qsTr("Waiting for its admin to let you in")
                        : preview.pageBoard
                        ? qsTr("Pages · %1 · %2 · edited %3").arg(Format.count(preview.columnCount, qsTr("column"), qsTr("columns"))).arg(Format.count(preview.pageCount, qsTr("page"), qsTr("pages"))).arg(Format.relativeTime(card.edited))
                        : qsTr("Infinite board · edited %1").arg(Format.relativeTime(card.edited))
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }

            RailButton {
                implicitWidth: 36
                implicitHeight: 36
                text: qsTr("Board options")
                iconPath: Icons.more
                onClicked: menu.popup()

                PaperMenu {
                    id: menu

                    PaperMenuItem {
                        text: card.admin ? qsTr("Share and projects…") : qsTr("Who has access…")
                        enabled: !card.pending
                        onTriggered: card.shareRequested(card.boardId)
                    }

                    PaperMenuItem {
                        text: qsTr("Rename…")
                        enabled: Library.isBoardAdmin(card.boardId)
                        onTriggered: card.renameRequested(card.boardId, card.name)
                    }

                    PaperMenuItem {
                        text: Library.isBoardAdmin(card.boardId) ? qsTr("Delete for everyone…") : qsTr("Remove from this device…")
                        destructive: true
                        onTriggered: card.deleteRequested(card.boardId, card.name)
                    }
                }
            }
        }
    }
}
