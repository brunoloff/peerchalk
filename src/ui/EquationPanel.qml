import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Edits one equation, showing it as it is typed.
Panel {
    id: panel

    signal accepted(string tex, bool display)
    signal cancelled()

    readonly property bool editing: visible && source.activeFocus

    // Shows an equation while the keyboard stays where it is.
    function show(tex: string, display: bool) {
        source.text = tex;
        displayStyle.checked = display;
        visible = true;
    }

    function focusSource() {
        source.forceActiveFocus();
        source.cursorPosition = source.length;
    }

    function open(tex: string, display: bool) {
        show(tex, display);
        focusSource();
    }

    function close() {
        visible = false;
    }

    function accept() {
        if (source.text.trim().length === 0 || formula.error.length > 0)
            return;
        close();
        accepted(source.text.trim(), displayStyle.checked);
    }

    visible: false
    implicitWidth: 380
    width: implicitWidth
    height: column.implicitHeight + 24
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Equation")

    // Clicks here must not reach the board underneath.
    MouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 8

        SectionLabel {
            text: qsTr("Equation")
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 90
            clip: true

            MathItem {
                id: formula

                readonly property real fit: Math.min(1, parent.width / Math.max(1, implicitWidth),
                                                     parent.height / Math.max(1, implicitHeight))

                anchors.centerIn: parent
                width: implicitWidth
                height: implicitHeight
                scale: fit
                tex: source.text
                displayMode: displayStyle.checked
                color: Theme.ink
                fontSize: 24
                Accessible.role: Accessible.Graphic
                Accessible.name: source.text
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.maximumHeight: 96

            TextArea {
                id: source

                placeholderText: qsTr("LaTeX, such as \\frac{a}{b}")
                font.family: Theme.monoFont
                font.pixelSize: 13
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                Accessible.name: qsTr("LaTeX source")
                Keys.onBacktabPressed: panel.accept()
                Keys.onEscapePressed: {
                    panel.close();
                    panel.cancelled();
                }
                Keys.onReturnPressed: event => {
                    if (event.modifiers & Qt.ShiftModifier)
                        event.accepted = false;
                    else
                        panel.accept();
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: formula.error.length > 0
            text: formula.error
            maximumLineCount: 3
            elide: Text.ElideRight
            color: "#B42318"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true

            CheckBox {
                id: displayStyle

                text: qsTr("Block equation (large, centered)")
                Layout.fillWidth: true
            }

            PaperButton {
                text: qsTr("Cancel")
                onClicked: {
                    panel.close();
                    panel.cancelled();
                }
            }

            PaperButton {
                text: qsTr("Done")
                enabled: source.text.trim().length > 0 && formula.error.length === 0
                onClicked: panel.accept()
            }
        }
    }
}
