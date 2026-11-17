import QtQuick
import QtQuick.Controls

// A short message at the bottom of the window, used for errors.
Rectangle {
    id: toast

    function show(message: string) {
        label.text = message;
        toast.opacity = 1;
        hideTimer.restart();
    }

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 24
    width: Math.min(label.implicitWidth + 32, parent.width - 32)
    height: label.implicitHeight + 20
    radius: Theme.controlRadius
    color: Theme.ink
    opacity: 0
    visible: opacity > 0
    z: 100

    Behavior on opacity {
        NumberAnimation {
            duration: 150
        }
    }

    Label {
        id: label

        anchors.centerIn: parent
        width: parent.width - 32
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: "white"
        Accessible.role: Accessible.AlertMessage
    }

    Timer {
        id: hideTimer

        interval: 4000
        onTriggered: toast.opacity = 0
    }
}
