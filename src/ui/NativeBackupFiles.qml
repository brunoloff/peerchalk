import QtQuick
import QtQuick.Dialogs

// The system's file dialogs for backups. Not part of the browser build.
Item {
    id: files

    signal saveChosen(url file)
    signal openChosen(url file)

    function save(name: string) {
        saveDialog.currentFile = name;
        saveDialog.open();
    }

    function choose() {
        openDialog.open();
    }

    FileDialog {
        id: saveDialog

        fileMode: FileDialog.SaveFile
        defaultSuffix: "peerchalk"
        nameFilters: [qsTr("PeerChalk backups (*.peerchalk)")]
        onAccepted: files.saveChosen(selectedFile)
    }

    FileDialog {
        id: openDialog

        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("PeerChalk backups (*.peerchalk)")]
        onAccepted: files.openChosen(selectedFile)
    }
}
