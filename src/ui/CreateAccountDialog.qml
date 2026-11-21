import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Creates a local account, making clear that nobody else keeps a copy.
PaperDialog {
    id: dialog

    signal submitted(string name)

    function ask() {
        nameField.clear();
        understood.checked = false;
        open();
        nameField.forceActiveFocus();
    }

    width: Math.min(500, parent.width - 32)
    title: qsTr("Create an account")
    onAccepted: dialog.submitted(nameField.text.trim())

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        PaperField {
            id: nameField

            Layout.fillWidth: true
            placeholderText: qsTr("Your name, as others will see it")
            Accessible.name: qsTr("Your name")
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: notice.implicitHeight + 24
            radius: Theme.controlRadius
            color: Theme.background
            border.color: Theme.border

            Label {
                id: notice

                anchors.fill: parent
                anchors.margins: 12
                wrapMode: Text.WordWrap
                color: Theme.ink
                text: qsTr("Your account exists only on this device. PeerChalk has no account server, so nobody can recover it or reset it for you. If you lose this device or clear this browser without a backup, your account is gone. You are responsible for backing it up from time to time.")
            }
        }

        CheckBox {
            id: understood

            Layout.fillWidth: true
            text: qsTr("I understand that keeping a backup is up to me")
        }
    }

    footer: PaperButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Create account")
            primary: true
            enabled: understood.checked && nameField.text.trim().length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
