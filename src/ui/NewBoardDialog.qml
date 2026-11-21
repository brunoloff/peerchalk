pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks for a new board's name and kind: infinite, or pages with their paper and background,
// or pages that start from a PDF.
PaperDialog {
    id: dialog

    property bool pages: pagesCard.checked
    property string paper: "A4"
    property bool landscape: false
    property string pageBackground: "blank"

    // Infinite boards: submitted(name, false, "", false, "").
    signal submitted(string name, bool pages, string paper, bool landscape, string background)
    signal pdfStartRequested(string name)

    function ask(initial: string) {
        nameField.text = initial;
        infiniteCard.checked = true;
        open();
        nameField.selectAll();
        nameField.forceActiveFocus();
    }

    readonly property string name: nameField.text.trim()

    width: Math.min(640, parent.width - 32)
    focus: true
    padding: 28
    topPadding: 24
    onAccepted: {
        if (dialog.name.length > 0)
            dialog.submitted(dialog.name, dialog.pages, dialog.paper, dialog.landscape,
                             dialog.pageBackground);
    }

    header: Item {}

    component FieldLabel: Label {
        font.pixelSize: 13
        font.weight: Font.Medium
        color: Theme.ink
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 18

        Label {
            text: qsTr("New board")
            font.family: Theme.headingFont
            font.pixelSize: 22
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        ColumnLayout {
            spacing: 6

            FieldLabel {
                text: qsTr("Name")
            }

            PaperField {
                id: nameField

                Layout.fillWidth: true
                placeholderText: qsTr("Board name")
                Accessible.name: qsTr("Name")
                onAccepted: dialog.accept()
            }
        }

        ButtonGroup {
            id: kinds
        }

        GridLayout {
            Layout.fillWidth: true
            columns: dialog.width > 520 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12

            BoardKindCard {
                id: infiniteCard

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                objectName: "infiniteKind"
                text: qsTr("Infinite board")
                summary: qsTr("Draw anywhere, in every direction. Exports as one large page.")
                checked: true
                ButtonGroup.group: kinds
            }

            BoardKindCard {
                id: pagesCard

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                objectName: "pagesKind"
                pages: true
                text: qsTr("Pages")
                summary: qsTr("Columns of real-size pages, with headings. Import PDFs to read and annotate.")
                ButtonGroup.group: kinds
            }
        }

        ColumnLayout {
            visible: dialog.pages
            spacing: 8

            FieldLabel {
                text: qsTr("Paper")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                ButtonGroup {
                    id: papers
                }

                Repeater {
                    model: ["A4", "Letter", "A5", "Legal"]

                    ChoiceChip {
                        required property string modelData

                        text: modelData
                        checked: dialog.paper === modelData
                        ButtonGroup.group: papers
                        onClicked: dialog.paper = modelData
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                PaperSegments {
                    choices: [{value: "portrait", text: qsTr("Portrait")},
                              {value: "landscape", text: qsTr("Landscape")}]
                    value: dialog.landscape ? "landscape" : "portrait"
                    onChosen: value => dialog.landscape = value === "landscape"
                }
            }
        }

        ColumnLayout {
            visible: dialog.pages
            spacing: 8

            FieldLabel {
                text: qsTr("Background")
            }

            BackgroundTiles {
                Layout.fillWidth: true
                value: dialog.pageBackground
                onChosen: value => dialog.pageBackground = value
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10

            PaperButton {
                visible: dialog.pages && Library.pdfSupported && Qt.platform.os !== "wasm"
                text: qsTr("Start from a PDF…")
                enabled: dialog.name.length > 0
                onClicked: {
                    dialog.close();
                    dialog.pdfStartRequested(dialog.name);
                }
            }

            Item {
                Layout.fillWidth: true
            }

            PaperButton {
                text: qsTr("Cancel")
                flat: true
                onClicked: dialog.reject()
            }

            PaperButton {
                text: qsTr("Create board")
                primary: true
                enabled: dialog.name.length > 0
                onClicked: dialog.accept()
            }
        }
    }
}
