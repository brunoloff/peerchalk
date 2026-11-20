pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The account screen's choice of account: the one used last as a large button, the others
// below it, and ways to add one. Wide screens show it as a card, phones without one.
Rectangle {
    id: chooser

    property bool framed: true
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    readonly property var lastAccount: Accounts.accounts.length > 0 ? Accounts.accounts[0] : null
    readonly property var otherAccounts: Accounts.accounts.slice(1)

    signal openRequested(string accountId)
    signal backupRequested(string accountId)
    signal removeRequested(var account)
    signal createRequested()
    signal linkRequested()
    signal importRequested()

    implicitHeight: framed ? 480 : content.implicitHeight
    color: framed ? Theme.surface : "transparent"
    border.color: Theme.border
    border.width: framed ? 1 : 0
    radius: 16

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: chooser.framed ? 28 : 0
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: Accounts.hasPendingBoardLink && Accounts.pendingBoardName.length > 0
                    ? qsTr("Join “%1”").arg(Accounts.pendingBoardName)
                    : chooser.lastAccount ? qsTr("Welcome back") : qsTr("Welcome to PeerChalk")
                wrapMode: Text.WordWrap
                font.family: Theme.headingFont
                font.pixelSize: chooser.framed ? 22 : 28
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            ReadyMark {
                visible: chooser.inBrowser && chooser.framed
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: -8
            visible: text.length > 0
            text: Accounts.hasPendingBoardLink ? qsTr("Choose who to join as.")
                : chooser.lastAccount ? "" : qsTr("Accounts are kept on this device, not on a server.")
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: Theme.muted
        }

        PaperButton {
            Layout.fillWidth: true
            visible: chooser.lastAccount === null
            primary: true
            implicitHeight: 52
            text: qsTr("Create a new account")
            onClicked: chooser.createRequested()
        }

        ContinueButton {
            objectName: "continueButton"
            Layout.fillWidth: true
            visible: chooser.lastAccount !== null
            account: chooser.lastAccount ?? {name: "", colorIndex: 0}
            prompt: Accounts.hasPendingBoardLink ? qsTr("Join as") : qsTr("Continue as")
            onClicked: chooser.openRequested(chooser.lastAccount.id)
            onBackupRequested: chooser.backupRequested(chooser.lastAccount.id)
            onRemoveRequested: chooser.removeRequested(chooser.lastAccount)
        }

        Label {
            visible: chooser.otherAccounts.length > 0
            text: qsTr("OTHER ACCOUNTS ON THIS DEVICE")
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.letterSpacing: 0.7
            color: Theme.muted
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: chooser.framed
            Layout.preferredHeight: chooser.framed ? -1 : contentHeight
            visible: chooser.otherAccounts.length > 0
            model: chooser.otherAccounts
            spacing: 8
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: AccountRow {
                required property var modelData

                width: ListView.view.width
                implicitHeight: 56
                account: modelData
                onOpenRequested: chooser.openRequested(modelData.id)
                onBackupRequested: chooser.backupRequested(modelData.id)
                onRemoveRequested: chooser.removeRequested(modelData)
            }
        }

        Item {
            Layout.fillHeight: true
            visible: chooser.framed && chooser.otherAccounts.length === 0
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // Three buttons share a row when their labels fit; otherwise "New account" takes a
        // row of its own, so no label is cut short.
        GridLayout {
            id: actions

            readonly property bool fits: newButton.implicitWidth + linkButton.implicitWidth
                + importButton.implicitWidth + 2 * columnSpacing <= width

            Layout.fillWidth: true
            columns: fits || !newButton.visible ? 3 : 2
            columnSpacing: 8
            rowSpacing: 8

            PaperButton {
                id: newButton

                Layout.fillWidth: true
                Layout.columnSpan: actions.fits ? 1 : 2
                visible: chooser.lastAccount !== null
                implicitHeight: 44
                text: qsTr("New account")
                onClicked: chooser.createRequested()
            }

            PaperButton {
                id: linkButton

                Layout.fillWidth: true
                Layout.preferredWidth: actions.fits ? implicitWidth : 1
                implicitHeight: 44
                text: qsTr("Link a device")
                onClicked: chooser.linkRequested()
            }

            PaperButton {
                id: importButton

                Layout.fillWidth: true
                Layout.preferredWidth: actions.fits ? implicitWidth : 1
                implicitHeight: 44
                text: qsTr("Import")
                onClicked: chooser.importRequested()
            }
        }

        CheckBox {
            Layout.leftMargin: -6
            visible: !chooser.inBrowser && chooser.lastAccount !== null
            text: qsTr("Open my last account directly next time")
            checked: Accounts.opensLastAccount
            font.pixelSize: 13
            onToggled: Accounts.opensLastAccount = checked
        }
    }
}
