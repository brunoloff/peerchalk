import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks the user to confirm something that cannot be undone.
Dialog {
    id: dialog

    property string message
    property string confirmText: qsTr("Delete")

    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent.width - 32)
    modal: true
    onAccepted: dialog.confirmed()

    // A layout filling the dialog sizes the wrapped text from the dialog's width; binding
    // the label's width directly makes its height and the dialog's feed each other.
    ColumnLayout {
        anchors.fill: parent

        Label {
            Layout.fillWidth: true
            text: dialog.message
            wrapMode: Text.WordWrap
            color: Theme.ink
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: dialog.confirmText
            destructive: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
