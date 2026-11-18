import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks for a backup's passphrase, twice when one is being chosen.
Dialog {
    id: dialog

    property bool choosing: false
    property string explanation
    readonly property bool valid: first.text.length >= (choosing ? Accounts.minPassphraseLength : 1)
        && (!choosing || first.text === second.text)

    signal submitted(string passphrase)

    function ask() {
        first.clear();
        second.clear();
        open();
        first.forceActiveFocus();
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent.width - 32)
    modal: true
    onAccepted: {
        const passphrase = first.text;
        first.clear();
        second.clear();
        dialog.submitted(passphrase);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: dialog.explanation
            wrapMode: Text.WordWrap
            color: Theme.muted
        }

        TextField {
            id: first

            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: dialog.choosing
                ? qsTr("Passphrase (at least %1 characters)").arg(Accounts.minPassphraseLength)
                : qsTr("Passphrase")
            Accessible.name: qsTr("Passphrase")
            onAccepted: if (dialog.valid && !dialog.choosing) dialog.accept()
        }

        TextField {
            id: second

            Layout.fillWidth: true
            visible: dialog.choosing
            echoMode: TextInput.Password
            placeholderText: qsTr("Repeat the passphrase")
            Accessible.name: qsTr("Repeat the passphrase")
            onAccepted: if (dialog.valid) dialog.accept()
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Continue")
            primary: true
            enabled: dialog.valid
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
