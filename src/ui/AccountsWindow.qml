pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The first screen: choose an account on this device, or add one.
ApplicationWindow {
    id: window

    property string createdAccountId

    width: 1280
    height: 800
    visible: true
    title: qsTr("PeerChalk")
    color: Theme.background
    font.family: Theme.bodyFont
    Component.onCompleted: {
        if (Accounts.hasPendingBoardLink && Accounts.accounts.length === 0)
            createDialog.ask();
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            x: Math.max(16, (scroll.availableWidth - width) / 2)
            y: Math.max(24, (window.height - implicitHeight) / 2)
            width: Math.min(520, scroll.availableWidth - 32)
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: Accounts.hasPendingBoardLink && Accounts.pendingBoardName.length > 0
                    ? qsTr("Join “%1”").arg(Accounts.pendingBoardName)
                    : Accounts.accounts.length > 0 ? qsTr("Choose an account") : qsTr("Welcome to PeerChalk")
                font.family: Theme.headingFont
                font.pixelSize: 28
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.muted
                text: Accounts.hasPendingBoardLink
                    ? qsTr("Choose who to join as. Accounts are kept on this device, not on a server.")
                    : qsTr("Accounts are kept on this device, not on a server.")
            }

            Repeater {
                model: Accounts.accounts

                AccountRow {
                    required property var modelData

                    Layout.fillWidth: true
                    account: modelData
                    onOpenRequested: Accounts.openAccount(account.id)
                    onBackupRequested: backups.exportAccount(account.id, "everything")
                    onRemoveRequested: removeDialog.ask(account)
                }
            }

            PaperButton {
                Layout.fillWidth: true
                primary: true
                text: qsTr("Create a new account")
                onClicked: createDialog.ask()
            }

            PaperButton {
                Layout.fillWidth: true
                text: qsTr("Link with another device")
                onClicked: linkDialog.ask("")
            }

            PaperButton {
                Layout.fillWidth: true
                text: qsTr("Import a backup")
                onClicked: backups.importAccount()
            }
        }
    }

    BackupFlow {
        id: backups

        onAccountImported: accountId => Accounts.openAccount(accountId)
        onExported: {
            if (window.createdAccountId.length > 0)
                Accounts.openAccount(window.createdAccountId);
        }
    }

    CreateAccountDialog {
        id: createDialog

        onSubmitted: name => {
            window.createdAccountId = Accounts.createAccount(name);
            if (window.createdAccountId.length === 0)
                return;
            // Someone following a link goes straight to the board; Settings offers backups.
            if (Accounts.hasPendingBoardLink)
                Accounts.openAccount(window.createdAccountId);
            else
                backupPrompt.open();
        }
    }

    Dialog {
        id: backupPrompt

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        modal: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Save your first backup?")
        onAccepted: backups.exportAccount(window.createdAccountId, "everything")
        onRejected: Accounts.openAccount(window.createdAccountId)

        ColumnLayout {
            anchors.fill: parent

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink
                text: qsTr("A backup is the only way to get this account back if this device or browser data is lost. You can also do this later in Settings → Account.")
            }
        }

        footer: DialogButtonBox {
            PaperButton {
                text: qsTr("Later")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            PaperButton {
                text: qsTr("Save a backup")
                primary: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    TextInputDialog {
        id: linkDialog

        title: qsTr("Link with another device")
        label: qsTr("On a device where you already use PeerChalk, open Settings → People, choose “Link another device” and paste its code here.")
        placeholder: "peerchalk-identity:1:…"
        acceptText: qsTr("Link")
        onSubmitted: text => {
            const id = Accounts.linkAccount(text);
            if (id.length > 0)
                Accounts.openAccount(id);
        }
    }

    RemoveAccountDialog {
        id: removeDialog

        onConfirmed: accountId => Accounts.removeAccount(accountId)
    }

    Toast {
        id: toast
    }

    Connections {
        target: Accounts

        function onErrorOccurred(message: string) {
            toast.show(message);
        }

        function onNoticeOccurred(message: string) {
            toast.show(message);
        }
    }
}
