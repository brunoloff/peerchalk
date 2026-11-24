import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A labelled setting of several lines, one entry per line, with an explanation underneath.
ColumnLayout {
    id: setting

    property string label
    property string hint
    property alias text: area.text
    property alias placeholder: area.placeholderText

    Layout.fillWidth: true
    spacing: 4

    Label {
        text: setting.label
        font.weight: Font.DemiBold
        color: Theme.ink
    }

    TextArea {
        id: area

        Layout.fillWidth: true
        Layout.minimumHeight: 96
        Accessible.name: setting.label
        font.family: Theme.monoFont
        font.pixelSize: 12
        wrapMode: TextEdit.NoWrap
        padding: 8
        color: Theme.ink
        placeholderTextColor: Theme.muted
        selectByMouse: true

        background: Rectangle { // framed like the one-line settings
            color: Theme.surface
            border.color: area.activeFocus ? Theme.accent : Theme.border
            border.width: 1
        }
    }

    Label {
        Layout.fillWidth: true
        text: setting.hint
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.muted
    }
}
