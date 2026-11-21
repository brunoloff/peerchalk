import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A row of a PaperMenu: its text, and on the right what it is set to (value), a check
// when it is the one chosen, or › when it opens a submenu. Destructive rows are red.
MenuItem {
    id: item

    property string value
    property bool destructive: false

    implicitHeight: 34
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 13
    font.weight: Font.Medium
    height: visible ? implicitHeight : 0 // hidden rows take no room

    indicator: Item {}
    arrow: Item {}

    contentItem: RowLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: item.text
            font: item.font
            elide: Text.ElideRight
            color: item.destructive ? Theme.danger : Theme.ink
            opacity: item.enabled ? 1 : 0.4
        }

        Label {
            readonly property string shown: item.subMenu !== null
                ? (item.subMenu as PaperMenu)?.value ?? "" : item.value

            visible: shown.length > 0 || item.subMenu !== null
            text: shown + (item.subMenu !== null ? "  ›" : "")
            font.pixelSize: 13
            color: Theme.muted
        }

        Icon {
            visible: item.checkable && item.checked
            implicitWidth: 16
            implicitHeight: 16
            path: Icons.check
            color: Theme.accent
            lineWidth: 2.2
        }
    }

    background: Rectangle {
        implicitWidth: 200
        radius: 7
        color: item.highlighted && item.enabled ? "#F1EFEA" : "transparent"
    }
}
