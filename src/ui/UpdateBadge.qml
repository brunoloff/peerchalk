import QtQuick
import QtQuick.Controls

// A small "Update" mark beside the app name, shown once a newer PeerChalk is found; it opens
// the same update steps as the account screen.
Button {
    id: badge

    readonly property int status: Updates.status

    visible: status === Updates.Status.Available || status === Updates.Status.Downloading
        || status === Updates.Status.Installed || (status === Updates.Status.Failed && details.opened)
    implicitHeight: 24
    leftPadding: 9
    rightPadding: 9
    topPadding: 0
    bottomPadding: 0
    text: status === Updates.Status.Installed ? qsTr("Restart") : qsTr("Update")
    font.pixelSize: 12
    font.weight: Font.DemiBold
    Accessible.name: qsTr("PeerChalk %1 is available").arg(Updates.latestVersion)
    ToolTip.visible: hovered && !details.opened
    ToolTip.text: Accessible.name
    ToolTip.delay: 400
    onClicked: details.opened ? details.close() : details.open()

    contentItem: Label {
        text: badge.text
        font: badge.font
        color: "white"
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: height / 2
        color: Theme.accent
        opacity: badge.down ? 0.85 : 1
    }

    Popup {
        id: details

        y: badge.height + 8
        width: 300
        padding: 16

        background: Rectangle {
            radius: 12
            color: Theme.surface
            border.color: Theme.border
        }

        contentItem: UpdateStatus {}
    }
}
