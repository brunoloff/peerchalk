import QtQuick.Controls

// What else can be done with an account on the account screen.
Menu {
    id: menu

    signal backupRequested()
    signal removeRequested()

    width: 250

    MenuItem {
        text: qsTr("Save a backup…")
        onTriggered: menu.backupRequested()
    }

    MenuItem {
        text: qsTr("Remove from this device…")
        onTriggered: menu.removeRequested()
    }
}
