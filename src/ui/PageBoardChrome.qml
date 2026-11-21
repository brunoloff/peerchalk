import QtQuick

// The column ruler of a page board and the dialogs its headings and subheadings ask with.
Item {
    id: chrome

    required property BoardCanvas canvas
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

    function addSubheading(columnId: string, before: string) {
        subheadingDialog.columnId = columnId;
        subheadingDialog.itemId = before;
        subheadingDialog.adding = true;
        subheadingDialog.title = qsTr("New subheading");
        subheadingDialog.acceptText = qsTr("Add");
        subheadingDialog.ask("");
    }

    // The ruler sits on the columns' heading band and stays below the board's title when the
    // band scrolls away, so the headings stay in sight far down a column.
    ColumnRuler {
        readonly property real stickyTop: Theme.gap + 60
        readonly property var first: chrome.canvas.pageColumns[0] ?? null
        readonly property real bandBottom: first === null ? 0
            : first.headingBottom * chrome.canvas.zoom + chrome.height / 2
              - chrome.canvas.viewCenter.y * chrome.canvas.zoom

        objectName: "columnRuler"
        anchors.left: parent.left
        anchors.right: parent.right
        y: Math.max(stickyTop, bandBottom - height - 8)
        visible: readable
        canvas: chrome.canvas
        onRenameRequested: (columnId, title) => {
            headingDialog.columnId = columnId;
            headingDialog.ask(title);
        }
        onImportRequested: before => chrome.importPdf(before)
        onDeleteRequested: (columnId, title) => {
            deleteDialog.columnId = columnId;
            deleteDialog.message = title.length > 0
                ? qsTr("Delete the column “%1” with all its pages and what is written on them?").arg(title)
                : qsTr("Delete this column with all its pages and what is written on them?");
            deleteDialog.open();
        }
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
