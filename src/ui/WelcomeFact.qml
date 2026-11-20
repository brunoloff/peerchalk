import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One point about PeerChalk on the account screen: an icon, a title and a sentence.
RowLayout {
    id: fact

    required property string icon
    required property string title
    required property string detail

    spacing: 12

    Icon {
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 2
        path: fact.icon
        color: Theme.accent
        lineWidth: 2
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: fact.title
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        Label {
            Layout.fillWidth: true
            text: fact.detail
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            lineHeight: 1.15
            color: Theme.muted
        }
    }
}
