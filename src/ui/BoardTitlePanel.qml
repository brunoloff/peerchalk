import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The board's name, who else is here, a way back to the project and the PDF export, at the
// top left of the board.
Panel {
    id: panel

    required property BoardCanvas canvas
    required property string boardName
    required property int others // other people on this board

    signal back()
    signal reported(string message)

    implicitWidth: titleColumn.implicitWidth + 20
    implicitHeight: titleColumn.implicitHeight + 12

    RowLayout {
        id: titleColumn

        anchors.centerIn: parent
        spacing: 8

        RailButton {
            implicitWidth: 36
            implicitHeight: 36
            text: qsTr("Back to %1").arg(Library.currentProjectName)
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
                text: panel.others === 0 ? qsTr("%1 · only you here").arg(Library.currentProjectName)
                    : panel.others === 1 ? qsTr("%1 · 1 other person here").arg(Library.currentProjectName)
                    : qsTr("%1 · %2 others here").arg(Library.currentProjectName).arg(panel.others)
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        PdfExportButton {
            canvas: panel.canvas
            title: panel.boardName
            onReported: message => panel.reported(message)
        }
    }
}
