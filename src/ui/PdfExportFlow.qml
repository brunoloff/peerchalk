import QtQuick

// Exports a board, or one column of a page board, as a PDF: asks where to save it (or
// downloads it in a browser) and reports how it went.
Item {
    id: flow

    required property BoardCanvas canvas
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    readonly property bool busy: exporter.busy
    // Untyped: NativeSaveFile does not exist in the browser build.
    readonly property var saveDialog: saveDialogLoader.item
    property string title

    signal reported(string message)

    // columnId: one column of a page board, or "" for everything; with it, first and last
    // pick its pages (from 1; 0 for all).
    function start(title: string, columnId: string, first: int, last: int) {
        flow.title = title;
        exporter.columnId = columnId;
        exporter.firstPage = first;
        exporter.lastPage = last;
        if (flow.inBrowser)
            exporter.download(title);
        else
            flow.saveDialog.save(exporter.fileName(title));
    }

    BoardExporter {
        id: exporter

        canvas: flow.canvas
        fontFamily: Theme.bodyFont
        onFinished: (ok, message) => flow.reported(message)
    }

    Loader {
        id: saveDialogLoader

        source: flow.inBrowser ? "" : "NativeSaveFile.qml"
        onLoaded: {
            item.nameFilters = [qsTr("PDF documents (*.pdf)")];
            item.defaultSuffix = "pdf";
        }
    }

    Connections {
        target: flow.saveDialog
        ignoreUnknownSignals: true

        function onChosen(file: url) {
            exporter.exportToFile(file, flow.title);
        }
    }
}
