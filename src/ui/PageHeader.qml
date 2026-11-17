import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A white title bar with a back button.
Rectangle {
    id: header

    property string title

    signal back()

    implicitHeight: 72
    color: Theme.surface

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 14

        RailButton {
            text: qsTr("Back")
            iconPath: Icons.chevronLeft
            onClicked: header.back()
        }

        Label {
            Layout.fillWidth: true
            text: header.title
            font.family: Theme.headingFont
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: Theme.ink
        }
    }
}
