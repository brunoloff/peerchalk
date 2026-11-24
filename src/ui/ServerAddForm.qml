import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Adding a server to a list: its kind, then its address. The list checks the address and
// shows what is wrong with it.
Rectangle {
    id: form

    required property var kinds
    required property var placeholderFor // function (kind) → an example address
    property string kind: kinds[0]

    signal addRequested(string kind, string address)
    signal canceled()

    function submit() {
        form.addRequested(form.kind, address.text);
    }

    implicitHeight: content.implicitHeight + 32
    color: "#FBFCFF"
    onVisibleChanged: {
        if (visible) {
            address.text = "";
            address.forceActiveFocus();
        }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "#EFECE6"
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        PaperSegments {
            objectName: "serverKind"
            value: form.kind
            choices: form.kinds.map(kind => ({ value: kind, text: kind.toUpperCase() }))
            onChosen: kind => form.kind = kind
        }

        PaperField {
            id: address

            objectName: "serverAddress"
            Layout.fillWidth: true
            placeholderText: form.placeholderFor(form.kind)
            font.family: Theme.monoFont
            font.pixelSize: 13
            Accessible.name: qsTr("Server address")
            onAccepted: form.submit()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            PaperButton {
                text: qsTr("Cancel")
                onClicked: form.canceled()
            }

            PaperButton {
                objectName: "confirmAddServer"
                text: qsTr("Add")
                primary: true
                enabled: address.text.trim().length > 0
                onClicked: form.submit()
            }
        }
    }
}
