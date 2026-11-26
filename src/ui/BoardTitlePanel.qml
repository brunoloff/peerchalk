import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The board's name, who else is here, a way back to the project (or to the app that embeds
// PeerChalk), the connection button and the PDF export, at the top left of the board.
Panel {
    id: panel

    required property BoardCanvas canvas
    required property string boardName
    required property string boardId
    required property int others // other people on this board
    property bool networkOpen: false
    // Network.boardConnection, for the board's network pane.
    readonly property alias connection: networkButton.status
    // Embedded, the project is the host's business: Back leads to the host, if it offers that.
    readonly property string backTarget: HostLink.embedded ? HostLink.backLabel : Library.currentProjectName
    readonly property string place: HostLink.embedded ? "" : Library.currentProjectName + " · "

    signal back()
    signal networkToggled()
    signal reported(string message)

    implicitWidth: titleColumn.implicitWidth + 20
    implicitHeight: titleColumn.implicitHeight + 12

    RowLayout {
        id: titleColumn

        anchors.centerIn: parent
        spacing: 8

        RailButton {
            visible: panel.backTarget.length > 0
            implicitWidth: 36
            implicitHeight: 36
            text: qsTr("Back to %1").arg(panel.backTarget)
            iconPath: Icons.chevronLeft
            onClicked: panel.back()
        }

        ColumnLayout {
            spacing: 0

            Label {
                text: panel.boardName
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Label {
                text: panel.place + (panel.others === 0 ? qsTr("only you here")
                    : panel.others === 1 ? qsTr("1 other person here")
                    : qsTr("%1 others here").arg(panel.others))
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        BoardNetworkButton {
            id: networkButton

            objectName: "boardNetworkButton"
            boardId: panel.boardId
            paneOpen: panel.networkOpen
            onClicked: panel.networkToggled()
        }

        PdfExportButton {
            canvas: panel.canvas
            title: panel.boardName
            onReported: message => panel.reported(message)
        }
    }
}
