import QtQuick

// The column ruler of a page board and the dialogs its headings and subheadings ask with.
Item {
    id: chrome

    required property BoardCanvas canvas

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
        onDeleteRequested: (columnId, title) => {
            deleteDialog.columnId = columnId;
            deleteDialog.message = title.length > 0
                ? qsTr("Delete the column “%1” with all its pages and what is written on them?").arg(title)
                : qsTr("Delete this column with all its pages and what is written on them?");
            deleteDialog.open();
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
