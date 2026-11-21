import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks for one line of text, such as a name or a pasted code.
PaperDialog {
    id: dialog

    property alias text: field.text
    property alias placeholder: field.placeholderText
    property string label
    property string acceptText: qsTr("Save")

    signal submitted(string text)

    function ask(initial: string) {
        field.text = initial;
        open();
        field.selectAll();
        field.forceActiveFocus();
    }

    width: Math.min(440, parent.width - 32)
    focus: true

    onAccepted: {
        if (field.text.trim().length > 0)
            dialog.submitted(field.text.trim());
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            Layout.fillWidth: true
            visible: dialog.label.length > 0
            text: dialog.label
            wrapMode: Text.WordWrap
            color: Theme.muted
        }

        PaperField {
            id: field

            Layout.fillWidth: true
            Accessible.name: dialog.title
            onAccepted: dialog.accept()
        }
    }

    footer: PaperButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: dialog.acceptText
            primary: true
            enabled: field.text.trim().length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
