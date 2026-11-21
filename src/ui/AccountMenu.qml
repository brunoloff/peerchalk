// What else can be done with an account on the account screen.
PaperMenu {
    id: menu

    signal backupRequested()
    signal removeRequested()

    width: 250

    PaperMenuItem {
        text: qsTr("Save a backup…")
        onTriggered: menu.backupRequested()
    }

    PaperMenuItem {
        text: qsTr("Remove from this device…")
        destructive: true
        onTriggered: menu.removeRequested()
    }
}
