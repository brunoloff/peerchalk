pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import PeerChalk

// The app window: a stack of pages that starts at the home screen.
ApplicationWindow {
    id: window

    function openBoard(boardId: string) {
        stack.push(boardPage, { boardId: boardId });
    }

    function openIdentity() {
        stack.push(identityPage);
    }

    width: 1280
    height: 800
    visible: true
    title: stack.depth > 1 && stack.currentItem.objectName === "boardPage"
        ? qsTr("%1 — PeerChalk").arg((stack.currentItem as BoardPage).boardName)
        : qsTr("PeerChalk")
    color: Theme.background
    font.family: Theme.bodyFont

    StackView {
        id: stack

        objectName: "stack"
        anchors.fill: parent
        initialItem: HomePage {
            onOpenBoard: boardId => window.openBoard(boardId)
            onOpenIdentity: window.openIdentity()
        }
    }

    Component {
        id: boardPage

        BoardPage {
            onBack: stack.pop()
        }
    }

    Component {
        id: identityPage

        IdentityPage {
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
}
