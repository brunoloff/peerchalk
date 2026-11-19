import QtQuick
import QtQuick.Dialogs

// The system's save dialog. Not part of the browser build.
FileDialog {
    id: dialog

    signal chosen(url file)

    function save(name: string) {
        currentFile = name;
        open();
    }

    fileMode: FileDialog.SaveFile
    onAccepted: dialog.chosen(selectedFile)
}
