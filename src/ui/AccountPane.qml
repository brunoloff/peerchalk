import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → Account: backups of this account, and how this device looks for updates.
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
                    text: qsTr("About")
                }

                SiteLinks {
                    Layout.fillWidth: true
                }

                SectionLabel {
                    Layout.topMargin: 12
                    visible: Qt.platform.os !== "wasm"
                    text: qsTr("Updates")
                }

                CheckBox {
                    Layout.leftMargin: -6
                    visible: Qt.platform.os !== "wasm"
                    text: qsTr("Check for a newer PeerChalk when the app starts")
                    checked: Accounts.checksForUpdates
                    onToggled: Accounts.checksForUpdates = checked
                }

                PaperButton {
                    Layout.fillWidth: true
                    visible: Qt.platform.os !== "wasm"
                    enabled: Updates.status !== Updates.Status.Checking
                        && Updates.status !== Updates.Status.Downloading
                    text: qsTr("Check for updates now")
                    onClicked: Updates.check()
                }

                UpdateStatus {
                    Layout.fillWidth: true
                }
            }
        }
    }

    BackupFlow {
        id: backups

        onPeopleImported: people => Library.importPeople(people)
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
