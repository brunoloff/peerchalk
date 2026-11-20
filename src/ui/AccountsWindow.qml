pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The first screen: what PeerChalk is, and a choice of account on this device or a new
// one. Wide windows put the two side by side, as the loading page in browsers does
// (deploy/web/site); narrow ones stack them and fold the explanation away.
ApplicationWindow {
    id: window

    property string createdAccountId
    readonly property bool wide: width >= 1140
    readonly property int pageMargin: wide ? 40 : 24

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
        contentHeight: page.implicitHeight + 2 * window.pageMargin

        Loader {
            id: page

            x: Math.max(16, (scroll.availableWidth - width) / 2)
            y: window.wide ? Math.max(window.pageMargin, (window.height - implicitHeight) / 2) : window.pageMargin
            width: window.wide ? Math.min(1104, scroll.availableWidth - 64) : Math.min(520, scroll.availableWidth - 32)
            sourceComponent: window.wide ? wideLayout : narrowLayout
        }
    }

    Component {
        id: wideLayout

        RowLayout {
            spacing: 64

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 24

                WelcomeLogo {
                    showJoke: Accounts.accounts.length === 0
                }

                WelcomeAbout {
                    Layout.fillWidth: true
                }
            }

            AccountChooser {
                Layout.preferredWidth: 400
                Layout.alignment: Qt.AlignVCenter
                onOpenRequested: accountId => Accounts.openAccount(accountId)
                onBackupRequested: accountId => backups.exportAccount(accountId, "everything")
                onRemoveRequested: account => removeDialog.ask(account)
                onCreateRequested: createDialog.ask()
                onLinkRequested: linkDialog.ask("")
                onImportRequested: backups.importAccount()
            }
        }
    }

    Component {
        id: narrowLayout

        ColumnLayout {
            spacing: 20

            Item {
                Layout.fillWidth: true
                implicitHeight: phoneLogo.implicitHeight

                WelcomeLogo {
                    id: phoneLogo

                    iconSize: 104
                    titleSize: 40
                    noteSize: 12
                    showSubtitle: false
                    showJoke: Accounts.accounts.length === 0
                }

                ReadyMark {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    visible: Qt.platform.os === "wasm"
                }
            }

            AccountChooser {
                Layout.fillWidth: true
                framed: false
                onOpenRequested: accountId => Accounts.openAccount(accountId)
                onBackupRequested: accountId => backups.exportAccount(accountId, "everything")
                onRemoveRequested: account => removeDialog.ask(account)
                onCreateRequested: createDialog.ask()
                onLinkRequested: linkDialog.ask("")
                onImportRequested: backups.importAccount()
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Theme.border
            }

            ItemDelegate {
                id: aboutToggle

                objectName: "aboutToggle"
                Layout.fillWidth: true
                Layout.topMargin: -12
                checkable: true
                text: qsTr("What is PeerChalk?")
                font.weight: Font.Medium
                leftPadding: 0
            }

            WelcomeAbout {
                Layout.fillWidth: true
                Layout.bottomMargin: 24
                visible: aboutToggle.checked
                factColumns: 1
                noteSize: 14
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
        target: Launcher

        function onErrorOccurred(message: string) {
            toast.show(message);
        }
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
