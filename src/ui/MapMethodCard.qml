import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A way of finding devices on the connection map: its kind, and whether its servers answer.
// Dashed when none does.
Rectangle {
    id: card

    // {kind, state, count}, from NetworkSummary.methods.
    required property var method
    readonly property bool down: method.state === "unreachable"

    radius: 10
    color: down ? "#FAF9F6" : Theme.surface
    border.color: down ? "#CFCAC0" : Theme.border
    Accessible.role: Accessible.StaticText
    Accessible.name: method.kind.toUpperCase() + ": " + status.text

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: 2

        ServerKindLabel {
            kind: card.method.kind
            color: card.down ? Theme.quiet : Theme.ink
        }

        RowLayout {
            spacing: 5

            Rectangle {
                implicitWidth: 7
                implicitHeight: 7
                radius: 4
                color: card.method.state === "working" ? Theme.working : "transparent"
                border.width: card.method.state === "working" ? 0 : 2
                border.color: card.down ? Theme.unreachable : Theme.lineFaint
            }

            Label {
                id: status

                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 12
                color: card.method.state === "working" ? Theme.workingInk
                     : card.down ? Theme.unreachable : Theme.quiet
                text: card.down ? qsTr("Down")
                    : card.method.state === "working" ? NetworkSummary.countText(card.method)
                    : card.method.state === "offline" ? qsTr("Offline") : qsTr("Checking…")
            }
        }
    }
}
