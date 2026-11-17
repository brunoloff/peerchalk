import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shows a code (an invite or a contact code) for the user to copy and send.
Dialog {
    id: dialog

    property string code
    property string explanation

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 32)
    modal: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: dialog.explanation
            wrapMode: Text.WordWrap
            color: Theme.muted
        }

        TextArea {
            Layout.fillWidth: true
            text: dialog.code
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            font.family: Theme.monoFont
            font.pixelSize: 13
            Accessible.name: dialog.title
            background: Rectangle {
                radius: Theme.controlRadius
                color: Theme.background
            }
        }
    }

    footer: DialogButtonBox {
        PaperButton {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        PaperButton {
            text: qsTr("Copy")
            primary: true
            onClicked: Library.copyToClipboard(dialog.code)
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
        }
    }
}
