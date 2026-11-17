import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Compares a person's fingerprint and records how they were verified.
Rectangle {
    id: panel

    required property string personKey
    required property string personName
    required property string fingerprint
    required property string verification
    readonly property bool verified: verification !== "none"
    readonly property var methodNames: ({
        "in-person": qsTr("in person"),
        "call": qsTr("on a call"),
        "shared": qsTr("through a trusted message")
    })

    signal removeRequested()

    implicitHeight: column.implicitHeight + 32
    radius: 12
    color: "transparent"
    border.color: Theme.border

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: panel.verified ? qsTr("%1 is verified %2").arg(panel.personName).arg(panel.methodNames[panel.verification])
                                 : qsTr("Verify %1").arg(panel.personName)
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        Label {
            Layout.fillWidth: true
            visible: !panel.verified
            text: qsTr("Compare this fingerprint with the one in %1's Settings → People. If every group matches, it really is their key.").arg(panel.personName)
            wrapMode: Text.WordWrap
            lineHeight: 1.3
            color: Theme.muted
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: theirs.implicitHeight + 20
            radius: Theme.controlRadius
            color: Theme.background

            Label {
                id: theirs

                anchors.fill: parent
                anchors.margins: 10
                text: panel.fingerprint
                wrapMode: Text.WordWrap
                font.family: Theme.monoFont
                font.pixelSize: 14
                font.letterSpacing: 1
                color: Theme.ink
            }
        }

        RowLayout {
            visible: !panel.verified
            spacing: 12

            Label {
                text: qsTr("We compared")
                color: Theme.muted
            }

            ComboBox {
                id: method

                textRole: "text"
                valueRole: "value"
                model: [
                    { value: "in-person", text: qsTr("in person") },
                    { value: "call", text: qsTr("on a call") },
                    { value: "shared", text: qsTr("through a trusted message") }
                ]
                Accessible.name: qsTr("How you compared")
            }
        }

        RowLayout {
            spacing: 8

            PaperButton {
                visible: !panel.verified
                text: qsTr("They match")
                primary: true
                onClicked: Library.setVerification(panel.personKey, method.currentValue)
            }

            PaperButton {
                visible: panel.verified
                text: qsTr("Mark as not verified")
                onClicked: Library.setVerification(panel.personKey, "none")
            }

            PaperButton {
                text: qsTr("Remove…")
                destructive: true
                onClicked: panel.removeRequested()
            }
        }
    }
}
