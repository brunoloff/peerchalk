import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → Account: backups of this account, and signing out.
Item {
    id: pane

    function exportBackup(kind: string) {
        Library.saveBoards();
        backups.exportAccount(Accounts.currentAccountId, kind);
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        Panel {
            x: 28
            y: 28
            width: Math.min(640, scroll.availableWidth - 56)
            implicitHeight: column.implicitHeight + 48

            ColumnLayout {
                id: column

                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                SectionLabel {
                    text: qsTr("Backups")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.ink
                    text: qsTr("Your account exists only on your devices. Save a backup from time to time and keep it somewhere safe.")
                }

                Label {
                    Layout.fillWidth: true
                    color: Theme.muted
                    text: isNaN(Accounts.currentLastBackup.getTime())
                        ? qsTr("No backup saved yet")
                        : qsTr("Last backup: %1").arg(Qt.formatDate(Accounts.currentLastBackup))
                }

                PaperButton {
                    Layout.fillWidth: true
                    primary: true
                    text: qsTr("Back up everything")
                    onClicked: pane.exportBackup("everything")
                }

                PaperButton {
                    Layout.fillWidth: true
                    text: qsTr("Back up identity and known people")
                    onClicked: pane.exportBackup("identity")
                }

                PaperButton {
                    Layout.fillWidth: true
                    text: qsTr("Export known people")
                    onClicked: pane.exportBackup("people")
                }

                PaperButton {
                    Layout.fillWidth: true
                    text: qsTr("Import known people")
                    onClicked: backups.importPeople()
                }

                SectionLabel {
                    Layout.topMargin: 12
                    text: qsTr("This device")
                }

                PaperButton {
                    Layout.fillWidth: true
                    text: qsTr("Sign out")
                    onClicked: signOutDialog.open()
                }
            }
        }
    }

    BackupFlow {
        id: backups

        onPeopleImported: people => Library.importPeople(people)
    }

    ConfirmDialog {
        id: signOutDialog

        title: qsTr("Sign out?")
        message: qsTr("You will go offline. The account stays on this device, and anyone using it can open it again. On a shared computer, save a backup and then remove the account from the account screen.")
        confirmText: qsTr("Sign out")
        onConfirmed: Accounts.signOut()
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
