import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → Network: whether PeerChalk goes online, the servers it uses to find and reach
// other devices (each checked as it is shown), and a live map of what it is connected to:
// beside the settings on wide screens, above them otherwise. Changes apply the next time
// PeerChalk starts.
Item {
    id: pane

    readonly property bool besideMap: width >= 1160

    // Checks the servers while the page is open.
    Timer {
        interval: 60 * 1000
        running: pane.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: Network.checkServers()
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth

        GridLayout {
            x: 32
            y: 28
            width: Math.min(pane.besideMap ? 1180 : 860, scroll.availableWidth - 64)
            columns: pane.besideMap ? 2 : 1
            columnSpacing: 40
            rowSpacing: 28

            Rectangle {
                id: mapCard

                Layout.row: 0
                Layout.column: pane.besideMap ? 1 : 0
                Layout.preferredWidth: pane.besideMap ? 480 : -1
                Layout.fillWidth: !pane.besideMap
                Layout.alignment: Qt.AlignTop
                implicitHeight: map.implicitHeight
                radius: Theme.panelRadius
                color: Theme.surface
                border.color: Theme.border

                NetworkMap {
                    id: map

                    objectName: "networkMap"
                    width: parent.width
                    wide: !pane.besideMap && width >= 720
                }
            }

            NetworkSettingsColumn {
                Layout.row: pane.besideMap ? 0 : 1
                Layout.column: 0
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.bottomMargin: 32
            }
        }
    }
}
