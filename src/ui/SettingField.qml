import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A labelled one-line text setting with an explanation underneath.
ColumnLayout {
    id: setting

    property string label
    property string hint
    property alias text: field.text
    property alias placeholder: field.placeholderText
    property bool secret: false

    Layout.fillWidth: true
    spacing: 4

    Label {
        text: setting.label
        font.weight: Font.DemiBold
        color: Theme.ink
    }

    TextField {
        id: field

        Layout.fillWidth: true
        Accessible.name: setting.label
        echoMode: setting.secret && !field.activeFocus ? TextInput.Password : TextInput.Normal
    }

    Label {
        Layout.fillWidth: true
        text: setting.hint
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.muted
    }
}
