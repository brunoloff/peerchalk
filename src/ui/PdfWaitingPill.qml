import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// On a page whose PDF has not reached this device: who it is coming from and how far it
// has got, or that it waits for someone who has it.
Item {
    id: pill

    property string hash
    // Library.blobProgressRevision makes this follow each piece that arrives.
    readonly property var progress: Library.blobProgressRevision >= 0 ? Library.blobProgress(hash) : ({})
    readonly property string from: progress.from ?? ""

    implicitWidth: row.implicitWidth + 18
    implicitHeight: 34
    width: implicitWidth
    height: implicitHeight

    SoftShadow {
        anchors.fill: parent
        radius: height / 2
        spread: 10
        strength: 0.12
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surface
        border.color: Theme.border
    }

    RowLayout {
        id: row

        anchors.centerIn: parent
        spacing: 8

        Avatar {
            visible: pill.from.length > 0
            name: pill.from
            size: 22
            color: Theme.personColors[1]
        }

        Label {
            text: pill.from.length > 0
                ? qsTr("Fetching the PDF from %1 · %2%").arg(pill.from).arg(pill.progress.percent)
                : qsTr("Waiting for someone who has this PDF")
            font.pixelSize: 12
            font.weight: Font.Medium
            color: Theme.ink
        }
    }
}
