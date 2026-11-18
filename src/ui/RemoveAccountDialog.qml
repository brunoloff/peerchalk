import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Confirms deleting an account from this device by having its name typed.
Dialog {
    id: dialog

    property var account: ({ name: "" })

    signal confirmed(string accountId)

    function ask(chosen: var) {
        account = chosen;
        field.clear();
        open();
        field.forceActiveFocus();
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent.width - 32)
    modal: true
    title: qsTr("Remove “%1” from this device?").arg(account.name)
    onAccepted: dialog.confirmed(account.id)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.ink
            text: qsTr("This deletes the account and its boards here. Without a backup or another linked device, it is lost for good. Type the account's name to confirm.")
        }

        TextField {
            id: field

            Layout.fillWidth: true
            placeholderText: dialog.account.name
            Accessible.name: qsTr("Account name")
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Remove")
            destructive: true
            enabled: field.text === dialog.account.name
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
