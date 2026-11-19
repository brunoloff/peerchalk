import QtQuick

// Exports the board as one PDF page sized to its contents.
RailButton {
    id: button

    required property BoardCanvas canvas
    required property string title
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    // Untyped: NativeSaveFile does not exist in the browser build.
    readonly property var saveDialog: saveDialogLoader.item

    signal reported(string message)

    implicitWidth: 36
    implicitHeight: 36
    text: qsTr("Export as PDF")
    iconPath: Icons.exportFile
    enabled: !exporter.busy && button.canvas.contentBounds.width > 0
    onClicked: {
        if (button.inBrowser)
            exporter.download(button.title);
        else
            button.saveDialog.save(exporter.fileName(button.title));
    }

    BoardExporter {
        id: exporter

        canvas: button.canvas
        fontFamily: Theme.bodyFont
        onFinished: (ok, message) => button.reported(message)
    }

    Loader {
        id: saveDialogLoader

        source: button.inBrowser ? "" : "NativeSaveFile.qml"
        onLoaded: {
            item.nameFilters = [qsTr("PDF documents (*.pdf)")];
            item.defaultSuffix = "pdf";
        }
    }

    Connections {
        target: button.saveDialog
        ignoreUnknownSignals: true

        function onChosen(file: url) {
            exporter.exportToFile(file, button.title);
        }
    }
}
