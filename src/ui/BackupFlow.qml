import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The steps of saving and restoring backups: passphrase, slow key derivation behind a
// "working" message, then the file. Browsers use downloads and their own file picker.
Item {
    id: flow

    readonly property bool inBrowser: Qt.platform.os === "wasm"
    property string accountId
    property string kind
    property string importMode
    property var pendingWork: null

    signal exported()
    signal accountImported(string accountId)
    signal peopleImported(var people)

    function exportAccount(id: string, backupKind: string) {
        accountId = id;
        kind = backupKind;
        exportPassphrase.ask();
    }

    function importAccount() {
        chooseFile("account");
    }

    function importPeople() {
        chooseFile("people");
    }

    function chooseFile(mode: string) {
        importMode = mode;
        if (inBrowser) {
            Accounts.pickImportFile();
        } else {
            nativePicker().choose();
        }
    }

    // Untyped: NativeBackupFiles does not exist in the browser build.
    function nativePicker(): var {
        nativeFiles.active = true;
        return nativeFiles.item;
    }

    function runSlowly(work: var) {
        pendingWork = work;
        working.open();
        workTimer.restart();
    }

    function saveFile() {
        if (inBrowser) {
            Accounts.downloadExport();
            flow.exported();
        } else {
            nativePicker().save(Accounts.exportFileName());
        }
    }

    Timer {
        id: workTimer

        interval: 80 // lets the "working" message appear before the interface is busy
        onTriggered: {
            const work = flow.pendingWork;
            flow.pendingWork = null;
            work();
            working.close();
        }
    }

    Connections {
        target: Accounts

        function onImportFileReady() {
            importPassphrase.ask();
        }
    }

    Loader {
        id: nativeFiles

        active: false
        source: flow.inBrowser ? "" : "NativeBackupFiles.qml"
    }

    Connections {
        target: nativeFiles.item
        ignoreUnknownSignals: true

        function onSaveChosen(file: url) {
            Accounts.saveExport(file);
            flow.exported();
        }

        function onOpenChosen(file: url) {
            Accounts.loadImportFile(file);
        }
    }

    PassphraseDialog {
        id: exportPassphrase

        title: qsTr("Protect this backup")
        choosing: true
        explanation: qsTr("The backup is encrypted with this passphrase. It cannot be recovered: without it, the backup is useless. Anyone with both the file and the passphrase can act as you.")
        onSubmitted: passphrase => flow.runSlowly(() => {
            if (Accounts.prepareExport(flow.accountId, flow.kind, passphrase))
                flow.saveFile();
        })
    }

    PassphraseDialog {
        id: importPassphrase

        title: qsTr("Open this backup")
        explanation: qsTr("Enter the passphrase chosen when the backup was saved.")
        onSubmitted: passphrase => flow.runSlowly(() => {
            if (flow.importMode === "people") {
                flow.peopleImported(Accounts.importPeople(passphrase));
            } else {
                const id = Accounts.importAccount(passphrase);
                if (id.length > 0)
                    flow.accountImported(id);
            }
        })
    }

    Dialog {
        id: working

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose

        RowLayout {
            spacing: 12

            BusyIndicator {
                running: working.visible
            }

            Label {
                text: qsTr("Working on the encryption…")
                color: Theme.ink
            }
        }
    }
}
