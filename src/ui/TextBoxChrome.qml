pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The frame, handles and formatting bar of the text box being edited, in screen coordinates.
Item {
    id: chrome

    required property BoardCanvas canvas
    required property TextBoxView view
    readonly property point corner: canvas.viewMatrix.map(Qt.point(view.boxX, view.boxY))
    readonly property real zoom: canvas.zoom
    readonly property real frameWidth: view.boxWidth * zoom
    readonly property real frameHeight: Math.max(view.height, view.boxFontSize * 1.3) * zoom
    readonly property TextEdit editor: view.editor
    readonly property int position: editor.cursorPosition
    readonly property RichTextBinding binding: view.binding
    // Where a new equation goes while no existing one is focused.
    property int insertPosition: 0

    // Back to the text, on the side of the equation the cursor came from.
    function returnToText(tex: var, display: bool) {
        const position = binding.focusedEquation;
        if (position >= 0) {
            const offset = binding.equationReturnPosition - position;
            binding.clearEquationFocus();
            const placed = tex !== null ? binding.replaceEquation(position, tex, display) : position;
            editor.cursorPosition = Math.max(placed, 0) + offset;
        } else if (tex !== null) {
            binding.insertEquation(insertPosition, tex, display);
        }
        editor.forceActiveFocus();
    }

    Connections {
        target: chrome.view

        function onEquationClicked(position: int) {
            chrome.binding.focusEquation(position, true);
            panel.focusSource();
        }

        function onEquationTabbed() {
            panel.focusSource();
        }
    }

    Connections {
        target: chrome.binding

        function onFocusedEquationChanged() {
            if (chrome.binding.focusedEquation < 0)
                panel.close();
            else if (!panel.editing)
                panel.show(chrome.binding.focusedEquationTex, chrome.binding.focusedEquationDisplay);
        }
    }

    Rectangle {
        x: chrome.corner.x - 5
        y: chrome.corner.y - 5
        width: chrome.frameWidth + 10
        height: chrome.frameHeight + 10
        color: "transparent"
        border.color: Theme.accent
        border.width: 1.5
        radius: 4
    }

    // Drag to move the box.
    Rectangle {
        x: chrome.corner.x - 5
        y: chrome.corner.y - 19
        width: 44
        height: 14
        radius: 4
        color: Theme.accent

        MouseArea {
            property point start
            property point origin

            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor
            onPressed: mouse => {
                start = mapToItem(chrome, mouse.x, mouse.y);
                origin = Qt.point(chrome.view.boxX, chrome.view.boxY);
            }
            onPositionChanged: mouse => {
                const now = mapToItem(chrome, mouse.x, mouse.y);
                chrome.canvas.moveTextBox(chrome.view.boxId,
                    Qt.point(origin.x + (now.x - start.x) / chrome.zoom,
                             origin.y + (now.y - start.y) / chrome.zoom));
            }
            onReleased: chrome.canvas.endTextBoxChange()
        }
    }

    // Drag to change the width.
    Rectangle {
        x: chrome.corner.x + chrome.frameWidth + 1
        y: chrome.corner.y + chrome.frameHeight / 2 - 12
        width: 8
        height: 24
        radius: 4
        color: Theme.accent

        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.SizeHorCursor
            onPositionChanged: mouse => {
                const right = mapToItem(chrome, mouse.x, mouse.y).x;
                chrome.canvas.setTextBoxWidth(chrome.view.boxId,
                                              (right - chrome.corner.x) / chrome.zoom);
            }
            onReleased: chrome.canvas.endTextBoxChange()
        }
    }

    Panel {
        x: Math.max(8, Math.min(chrome.corner.x + 48, chrome.width - width - 8))
        y: Math.max(8, chrome.corner.y - height - 12)
        width: bar.implicitWidth + 8
        height: bar.implicitHeight + 8
        radius: 12

        RowLayout {
            id: bar

            anchors.centerIn: parent
            spacing: 2

            component BarButton: RailButton {
                implicitWidth: 36
                implicitHeight: 36
                focusPolicy: Qt.NoFocus
            }

            BarButton {
                text: qsTr("Bold (Ctrl+B)")
                iconPath: Icons.bold
                selected: chrome.binding.equations && chrome.binding.isBold(chrome.position - 1)
                onClicked: chrome.binding.toggleBold(chrome.editor.selectionStart,
                                                          chrome.editor.selectionEnd)
            }

            BarButton {
                text: qsTr("Italic (Ctrl+I)")
                iconPath: Icons.italic
                selected: chrome.binding.equations && chrome.binding.isItalic(chrome.position - 1)
                onClicked: chrome.binding.toggleItalic(chrome.editor.selectionStart,
                                                            chrome.editor.selectionEnd)
            }

            BarButton {
                text: qsTr("Equation (or type $x^2$, or $$x^2$$ for a block)")
                iconPath: Icons.equation
                onClicked: {
                    chrome.binding.clearEquationFocus();
                    chrome.insertPosition = chrome.position;
                    panel.open("", false);
                }
            }

            BarButton {
                text: qsTr("Smaller text")
                iconPath: Icons.minus
                onClicked: chrome.canvas.setTextBoxFontSize(chrome.view.boxId,
                                                            chrome.view.boxFontSize / 1.2)
            }

            BarButton {
                text: qsTr("Larger text")
                iconPath: Icons.plus
                onClicked: chrome.canvas.setTextBoxFontSize(chrome.view.boxId,
                                                            chrome.view.boxFontSize * 1.2)
            }

            BarButton {
                text: qsTr("Use the pen colour")
                iconPath: Icons.pen
                onClicked: chrome.canvas.setTextBoxColor(chrome.view.boxId, chrome.canvas.penColor)
            }

            BarButton {
                text: qsTr("Delete text box")
                iconPath: Icons.trash
                onClicked: chrome.canvas.removeTextBox(chrome.view.boxId)
            }
        }
    }

    EquationPanel {
        id: panel

        x: Math.max(8, Math.min(chrome.corner.x, chrome.width - width - 8))
        y: Math.min(chrome.corner.y + chrome.frameHeight + 14, chrome.height - height - 8)
        onAccepted: (tex, display) => chrome.returnToText(tex, display)
        onCancelled: chrome.returnToText(null, false)
    }
}
