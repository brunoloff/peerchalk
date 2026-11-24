import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The bottom of a server list: "Add server" and "Reset to defaults", which asks first.
Rectangle {
    id: footer

    property bool adding
    property bool confirmingReset

    signal addRequested()
    signal resetAsked()
    signal resetConfirmed()
    signal resetCanceled()

    implicitHeight: row.implicitHeight + 16
    color: "#FAF9F6"

    Rectangle {
        width: parent.width
        height: 1
        color: "#EFECE6"
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        PaperButton {
            objectName: "addServer"
            visible: !footer.adding && !footer.confirmingReset
            implicitHeight: 34
            text: qsTr("Add server")
            onClicked: footer.addRequested()
        }

        Label {
            Layout.fillWidth: true
            visible: footer.confirmingReset
            leftPadding: 6
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: Theme.ink
            text: qsTr("Reset to PeerChalk's defaults? Servers you added are removed.")
        }

        Item {
            Layout.fillWidth: true
            visible: !footer.confirmingReset
        }

        PaperButton {
            visible: footer.confirmingReset
            implicitHeight: 34
            text: qsTr("Cancel")
            onClicked: footer.resetCanceled()
        }

        PaperButton {
            objectName: footer.confirmingReset ? "confirmReset" : "resetServers"
            implicitHeight: 34
            flat: !footer.confirmingReset
            strong: footer.confirmingReset
            text: footer.confirmingReset ? qsTr("Reset") : qsTr("Reset to defaults")
            onClicked: footer.confirmingReset ? footer.resetConfirmed() : footer.resetAsked()
        }
    }
}
