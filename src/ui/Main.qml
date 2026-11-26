pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import PeerChalk

// The app window: a stack of pages that starts at the home screen.
ApplicationWindow {
    id: window

    function openBoard(boardId: string) {
        // Embedded, the board is all there is: it appears at once, without sliding over home.
        stack.push(boardPage, { boardId: boardId },
                   HostLink.embedded ? StackView.Immediate : StackView.PushTransition);
    }

    // Opens the board link this app was started with, if any.
    function openPendingBoardLink() {
        const link = Accounts.takePendingBoardLink();
        if (link.length === 0)
            return;
        const boardId = Library.openBoardLink(link);
        if (boardId.length > 0)
            window.openBoard(boardId);
    }

    function openSettings(tab: int) {
        stack.push(settingsPage, { currentTab: tab });
    }

    width: 1280
    height: 800
    visible: true
    title: stack.depth > 1 && stack.currentItem.objectName === "boardPage"
        ? qsTr("%1 — PeerChalk").arg((stack.currentItem as BoardPage).boardName)
        : qsTr("PeerChalk")
    color: Theme.background
    font.family: Theme.bodyFont
    Component.onCompleted: Qt.callLater(window.openPendingBoardLink)

    StackView {
        id: stack

        objectName: "stack"
        anchors.fill: parent
        initialItem: HomePage {
            onOpenBoard: boardId => window.openBoard(boardId)
            onOpenSettings: tab => window.openSettings(tab)
        }
    }

    Component {
        id: boardPage

        BoardPage {
            // Embedded, Back leaves the board for the host's page; there is no home screen.
            onBack: HostLink.embedded ? HostLink.back() : stack.pop()
        }
    }

    Component {
        id: settingsPage

        SettingsPage {
            onBack: stack.pop()
        }
    }

    Toast {
        id: toast
    }

    Connections {
        target: Library

        function onErrorOccurred(message: string) {
            toast.show(message);
        }
    }

    Connections {
        target: Network

        function onJoinRequested(name: string, itemName: string) {
            toast.show(qsTr("%1 asks to join “%2”. Approve them under Requests.").arg(name).arg(itemName));
        }
    }
}
