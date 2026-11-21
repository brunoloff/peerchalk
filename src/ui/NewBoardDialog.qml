pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks for a new board's name and kind: infinite, or pages with their paper and background.
Dialog {
    id: dialog

    property alias name: nameField.text
    property bool pages: pagesCard.checked
    property string paper: "A4"
    property bool landscape: false
    property string pageBackground: "blank"

    // Infinite boards: submitted(name, false, "", false, "").
    signal submitted(string name, bool pages, string paper, bool landscape, string background)

    function ask(initial: string) {
        nameField.text = initial;
        infiniteCard.checked = true;
        open();
        nameField.selectAll();
        nameField.forceActiveFocus();
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 32)
    modal: true
    focus: true
    title: qsTr("New board")
    onAccepted: {
        if (nameField.text.trim().length > 0)
            dialog.submitted(nameField.text.trim(), dialog.pages, dialog.paper, dialog.landscape,
                             dialog.pageBackground);
    }

    component ChoiceRow: ColumnLayout {
        id: row

        property string label
        property var choices // [{value, text}]
        property string value

        signal chosen(string value)

        Layout.fillWidth: true
        spacing: 6

        Label {
            text: row.label
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.ink
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            ButtonGroup {
                id: group
            }

            Repeater {
                model: row.choices

                ChoiceChip {
                    required property var modelData

                    text: modelData.text
                    checked: row.value === modelData.value
                    ButtonGroup.group: group
                    onClicked: row.chosen(modelData.value)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        TextField {
            id: nameField

            Layout.fillWidth: true
            placeholderText: qsTr("Board name")
            Accessible.name: qsTr("Name")
            onAccepted: dialog.accept()
        }

        ButtonGroup {
            id: kinds
        }

        BoardKindCard {
            id: infiniteCard

            Layout.fillWidth: true
            objectName: "infiniteKind"
            text: qsTr("Infinite board")
            summary: qsTr("Draw anywhere, in every direction. Exports as one large page.")
            checked: true
            ButtonGroup.group: kinds
        }

        BoardKindCard {
            id: pagesCard

            Layout.fillWidth: true
            objectName: "pagesKind"
            text: qsTr("Pages")
            summary: qsTr("Columns of real-size pages, with headings. Import PDFs to read and annotate.")
            ButtonGroup.group: kinds
        }

        ChoiceRow {
            visible: dialog.pages
            label: qsTr("Paper")
            value: dialog.paper
            choices: [{value: "A4", text: "A4"}, {value: "Letter", text: "Letter"},
                      {value: "A5", text: "A5"}, {value: "Legal", text: "Legal"}]
            onChosen: value => dialog.paper = value
        }

        ChoiceRow {
            visible: dialog.pages
            label: qsTr("Orientation")
            value: dialog.landscape ? "landscape" : "portrait"
            choices: [{value: "portrait", text: qsTr("Portrait")},
                      {value: "landscape", text: qsTr("Landscape")}]
            onChosen: value => dialog.landscape = value === "landscape"
        }

        ChoiceRow {
            visible: dialog.pages
            label: qsTr("Background")
            value: dialog.pageBackground
            choices: [{value: "blank", text: qsTr("Blank")}, {value: "ruled", text: qsTr("Ruled")},
                      {value: "squared", text: qsTr("Squared")},
                      {value: "dotted", text: qsTr("Dotted")}]
            onChosen: value => dialog.pageBackground = value
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Create board")
            primary: true
            enabled: nameField.text.trim().length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
