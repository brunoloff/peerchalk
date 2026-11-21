pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks which pages of a column to export: all of them, or a range.
PaperDialog {
    id: dialog

    property string columnId
    property string columnTitle
    property int pageCount: 1

    // first and last are 0 for every page.
    signal chosen(string columnId, string title, int first, int last)

    function ask(columnId: string, title: string, pages: int, current: int) {
        dialog.columnId = columnId;
        dialog.columnTitle = title;
        dialog.pageCount = Math.max(1, pages);
        allPages.checked = true;
        fromBox.value = Math.max(1, current);
        toBox.value = Math.max(1, current);
        open();
    }

    title: qsTr("Export “%1”").arg(columnTitle.length > 0 ? columnTitle : qsTr("Untitled column"))
    width: Math.min(440, parent.width - 32)
    onAccepted: {
        if (allPages.checked)
            dialog.chosen(columnId, columnTitle, 0, 0);
        else
            dialog.chosen(columnId, columnTitle, Math.min(fromBox.value, toBox.value),
                          Math.max(fromBox.value, toBox.value));
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RadioButton {
            id: allPages

            text: dialog.pageCount === 1 ? qsTr("The page") : qsTr("All %1 pages").arg(dialog.pageCount)
            checked: true
        }

        RowLayout {
            spacing: 8

            RadioButton {
                id: somePages

                text: qsTr("Pages")
            }

            SpinBox {
                id: fromBox

                from: 1
                to: dialog.pageCount
                editable: true
                enabled: somePages.checked
                Accessible.name: qsTr("First page")
            }

            Label {
                text: qsTr("to")
                color: Theme.muted
            }

            SpinBox {
                id: toBox

                from: 1
                to: dialog.pageCount
                editable: true
                enabled: somePages.checked
                Accessible.name: qsTr("Last page")
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Each page keeps its exact size. Headings and subheadings become bookmarks.")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.muted
        }
    }

    footer: PaperButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Export")
            primary: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
