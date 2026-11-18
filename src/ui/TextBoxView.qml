pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// One text box, in world coordinates. It takes pointer input only while being edited, so
// drawing passes through it otherwise.
Item {
    id: view

    required property string boxId
    required property real boxX
    required property real boxY
    required property real boxWidth
    required property real boxFontSize
    required property color boxColor
    required property BoardCanvas canvas
    readonly property bool editing: canvas.editingTextBox === boxId
    readonly property alias editor: edit
    readonly property alias binding: richText

    signal equationClicked(int position)
    signal equationTabbed()

    x: boxX
    y: boxY
    width: boxWidth
    height: edit.contentHeight
    onEditingChanged: {
        if (editing) {
            edit.forceActiveFocus();
        } else {
            richText.clearEquationFocus();
            richText.flush();
            edit.deselect();
        }
    }
    Component.onCompleted: if (editing) edit.forceActiveFocus()

    TextEdit {
        id: edit

        width: parent.width
        enabled: view.editing
        activeFocusOnPress: true
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.Wrap
        selectByMouse: true
        persistentSelection: true
        color: view.boxColor
        selectionColor: Theme.accentSoft
        selectedTextColor: view.boxColor
        font.family: Theme.bodyFont
        font.pixelSize: view.boxFontSize
        Accessible.name: qsTr("Text box")
        onCursorPositionChanged: {
            if (richText.focusedEquation >= 0 && cursorPosition !== richText.equationReturnPosition)
                richText.clearEquationFocus();
        }

        // Keys while the cursor is stopped at an equation; returns whether one was used.
        function handleEquationKey(event: KeyEvent): bool {
            if (richText.focusedEquation < 0)
                return false;
            if (event.key === Qt.Key_Tab) {
                view.equationTabbed();
            } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                richText.removeFocusedEquation();
            } else if (event.key === Qt.Key_Escape) {
                richText.clearEquationFocus();
            } else {
                if (event.text.length > 0)
                    richText.clearEquationFocus(); // typing continues beside the equation
                return false;
            }
            return true;
        }

        function handleArrow(event: KeyEvent): bool {
            const left = event.key === Qt.Key_Left;
            if ((!left && event.key !== Qt.Key_Right) || event.modifiers !== Qt.NoModifier
                    || selectedText.length > 0)
                return false;
            const target = richText.moveAcrossEquation(cursorPosition, left ? -1 : 1);
            if (target < 0)
                return false;
            cursorPosition = target;
            return true;
        }

        Keys.onPressed: event => {
            const control = (event.modifiers & Qt.ControlModifier) !== 0;
            if (handleArrow(event) || handleEquationKey(event)) {
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Escape) {
                view.canvas.finishTextEditing();
            } else if (control && event.key === Qt.Key_B) {
                richText.toggleBold(selectionStart, selectionEnd);
            } else if (control && event.key === Qt.Key_I) {
                richText.toggleItalic(selectionStart, selectionEnd);
            } else if (event.matches(StandardKey.Undo)) {
                richText.flush();
                view.canvas.undo();
            } else {
                return;
            }
            event.accepted = true;
        }
    }

    RichTextBinding {
        id: richText

        canvas: view.canvas
        boxId: view.boxId
        fontSize: view.boxFontSize
        document: edit.textDocument
        onCursorRequested: position => edit.cursorPosition = position
    }

    Repeater {
        model: richText.equations.length

        delegate: Item {
            id: equation

            required property int index
            readonly property var info: richText.equations[index] ?? null

            x: info?.x ?? 0
            y: info?.y ?? 0
            width: info?.failed ? failedText.implicitWidth : formula.implicitWidth
            height: info?.failed ? failedText.implicitHeight : formula.implicitHeight

            Rectangle {
                readonly property real pixel: 1 / view.canvas.zoom

                visible: richText.focusedEquation === (equation.info?.position ?? -1)
                anchors.fill: parent
                anchors.margins: -3 * pixel
                color: Theme.accentSoft
                border.color: Theme.accent
                border.width: 1.5 * pixel
                radius: 3 * pixel
            }

            MathItem {
                id: formula

                visible: !equation.info?.failed
                anchors.fill: parent
                tex: equation.info?.tex ?? ""
                displayMode: equation.info?.display ?? false
                color: view.boxColor
                fontSize: view.boxFontSize
                viewScale: view.canvas.zoom
            }

            Text {
                id: failedText

                visible: equation.info?.failed ?? false
                text: equation.info?.tex ?? ""
                color: "#B42318"
                font.family: Theme.monoFont
                font.pixelSize: view.boxFontSize * 0.8
            }

            TapHandler {
                enabled: view.editing
                onTapped: view.equationClicked(equation.info.position)
            }

            HoverHandler {
                enabled: view.editing
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
