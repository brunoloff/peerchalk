import QtQuick
import QtQuick.Dialogs

// The system's file dialog for choosing a PDF to import. Not part of the browser build.
Item {
    id: files

    signal chosen(url file)

    function choose() {
        dialog.open();
    }

    FileDialog {
        id: dialog

        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("PDF documents (*.pdf)")]
        onAccepted: files.chosen(selectedFile)
    }
}
