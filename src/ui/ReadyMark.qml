import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Says that the browser's loading has finished, where the loading page showed progress.
RowLayout {
    spacing: 6

    Icon {
        implicitWidth: 15
        implicitHeight: 15
        path: Icons.check
        color: Theme.success
        lineWidth: 2.5
    }

    Label {
        text: qsTr("Ready")
        font.pixelSize: 13
        font.weight: Font.Medium
        color: Theme.success
    }
}
