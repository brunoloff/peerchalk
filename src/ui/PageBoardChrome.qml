import QtQuick

// The column ruler of a page board and the dialogs its headings and subheadings ask with.
Item {
    id: chrome

    required property BoardCanvas canvas
    // Where the ruler ends: the board opens below it.
    readonly property real rulerBottom: ruler.y + ruler.height
    required property string boardId
    // Where an imported PDF goes: before this column, or "" for the end.
    property string importBefore
    property bool inBrowser: Qt.platform.os === "wasm"

    signal reported(string message)

    function importPdf(before: string) {
        chrome.importBefore = before;
        const picker = chrome.pdfPicker();
        if (picker !== null)
            picker.choose();
    }

    // Untyped: NativePdfFile does not exist in the browser build.
    function pdfPicker(): var {
        return pdfFiles.item;
    }

    function renameSubheading(columnId: string, itemId: string, text: string) {
        subheadingDialog.columnId = columnId;
        subheadingDialog.itemId = itemId;
        subheadingDialog.adding = false;
        subheadingDialog.title = qsTr("Rename subheading");
        subheadingDialog.acceptText = qsTr("Save");
        subheadingDialog.ask(text);
    }

    function exportColumn(columnId: string, title: string) {
        columnExport.start(title.length > 0 ? title : qsTr("Column"), columnId);
    }

    function addSubheading(columnId: string, before: string) {
        subheadingDialog.columnId = columnId;
        subheadingDialog.itemId = before;
        subheadingDialog.adding = true;
        subheadingDialog.title = qsTr("New subheading");
        subheadingDialog.acceptText = qsTr("Add");
        subheadingDialog.ask("");
    }

    // The desk fades out under the ruler, so pages slide under it rather than meeting an edge.
    Rectangle {
        width: parent.width
        height: chrome.rulerBottom + 40
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Theme.desk
            }
            GradientStop {
                position: (chrome.rulerBottom) / (chrome.rulerBottom + 40)
                color: Theme.desk
            }
            GradientStop {
                position: 1
                color: Qt.rgba(0.925, 0.918, 0.894, 0)
            }
        }
    }

    // Pinned below the board's title, in step with the columns.
    ColumnRuler {
        id: ruler

        objectName: "columnRuler"
        anchors.left: parent.left
        anchors.right: parent.right
        y: Theme.gap + 60
        canvas: chrome.canvas
        onRenameRequested: (columnId, title) => {
            headingDialog.columnId = columnId;
            headingDialog.ask(title);
        }
        onImportRequested: before => chrome.importPdf(before)
        onExportRequested: (columnId, title) => chrome.exportColumn(columnId, title)
        onDeleteRequested: (columnId, title) => {
            deleteDialog.columnId = columnId;
            deleteDialog.message = title.length > 0
                ? qsTr("Delete the column “%1” with all its pages and what is written on them?").arg(title)
                : qsTr("Delete this column with all its pages and what is written on them?");
            deleteDialog.open();
        }
    }

    PdfExportFlow {
        id: columnExport

        canvas: chrome.canvas
        onReported: message => chrome.reported(message)
    }

    // Untyped: NativePdfFile does not exist in the browser build.
    Loader {
        id: pdfFiles

        source: chrome.inBrowser ? "" : "NativePdfFile.qml"
    }

    Connections {
        target: pdfFiles.item
        ignoreUnknownSignals: true

        function onChosen(file: url) {
            if (Library.importPdf(chrome.boardId, file, chrome.importBefore))
                chrome.reported(qsTr("PDF added as a new column"));
        }
    }

    TextInputDialog {
        id: headingDialog

        property string columnId

        title: qsTr("Rename heading")
        placeholder: qsTr("Heading")
        onSubmitted: text => chrome.canvas.setColumnTitle(columnId, text)
    }

    TextInputDialog {
        id: subheadingDialog

        property string columnId
        property string itemId // the subheading, or when adding the item it goes before
        property bool adding

        placeholder: qsTr("Subheading")
        onSubmitted: text => {
            if (adding)
                chrome.canvas.addSubheading(columnId, itemId, text);
            else
                chrome.canvas.setSubheadingText(columnId, itemId, text);
        }
    }

    ConfirmDialog {
        id: deleteDialog

        property string columnId

        title: qsTr("Delete column")
        message: ""
        confirmText: qsTr("Delete column")
        onConfirmed: chrome.canvas.removeColumn(columnId)
    }
}
