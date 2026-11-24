import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the last check found for a server: a dot (filled when it answers, a ring when it does
// not) and a few words.
RowLayout {
    id: status

    // {state: "checking" | "working" | "unreachable", milliseconds}, or undefined before any
    // check.
    property var result
    readonly property string phase: result ? result.state : "checking"
    property int textSize: 12

    spacing: 6
    Accessible.role: Accessible.StaticText
    Accessible.name: words.text

    Rectangle {
        implicitWidth: 8
        implicitHeight: 8
        radius: 4
        color: status.phase === "working" ? Theme.working : "transparent"
        border.width: status.phase === "working" ? 0 : 2
        border.color: status.phase === "unreachable" ? Theme.unreachable : Theme.lineFaint
    }

    Label {
        id: words

        font.pixelSize: status.textSize
        color: status.phase === "working" ? Theme.workingInk
             : status.phase === "unreachable" ? Theme.unreachable : Theme.quiet
        text: status.phase === "working" ? qsTr("Working · %1 ms").arg(status.result.milliseconds)
            : status.phase === "unreachable" ? qsTr("Not reachable") : qsTr("Checking…")
    }
}
