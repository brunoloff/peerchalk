import QtQuick
import QtQuick.Controls

// The network button beside the board's title: a dot on it tells at a glance whether this
// device reaches the others on the board; clicking opens the board's network pane. It also
// keeps the status the pane shows, refreshed every second or two.
RailButton {
    id: button

    required property string boardId
    property bool paneOpen: false
    // Network.boardConnection(boardId), refreshed while the board is open.
    property var status: ({ state: "offline", facts: {}, people: [], repository: { open: false } })
    readonly property string connectionState: status.state

    function refresh() {
        status = Network.boardConnection(boardId);
    }

    implicitWidth: 36
    implicitHeight: 36
    iconPath: Icons.peers
    selected: paneOpen
    text: qsTr("Connection: %1").arg(ConnectionAdvice.title(connectionState, status.facts))
    Accessible.description: ConnectionAdvice.detail(connectionState, status.facts)
    Component.onCompleted: refresh()

    Timer {
        interval: button.paneOpen ? 1000 : 2000
        running: button.visible // the phone's and the desktop's buttons are never both shown
        repeat: true
        onTriggered: button.refresh()
    }

    Rectangle {
        objectName: "connectionDot"
        z: 1 // above the button's icon
        x: parent.width - width - 5
        y: 5
        width: 9
        height: 9
        radius: 4.5
        color: ConnectionAdvice.color(button.connectionState)
        border.color: Theme.surface
        border.width: 1.5
    }
}
