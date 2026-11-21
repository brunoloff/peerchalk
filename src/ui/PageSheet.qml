import QtQuick
import QtQuick.Controls

// On a phone: the page in view and its column's settings, in a sheet from the bottom.
Drawer {
    id: sheet

    required property BoardCanvas canvas

    signal exportRequested(string columnId, string title)

    edge: Qt.BottomEdge
    width: parent.width
    height: Math.min(parent.height * 0.8, content.implicitHeight + 64)
    modal: true

    background: Rectangle {
        color: Theme.surface
        radius: 18

        Rectangle { // squares off the bottom corners
            anchors.bottom: parent.bottom
            width: parent.width
            height: 18
            color: Theme.surface
        }

        Rectangle { // the handle
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            width: 36
            height: 4
            radius: 2
            color: Theme.border
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: 24
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        PageInspector {
            id: content

            width: parent.width
            canvas: sheet.canvas
            onExportRequested: (columnId, title) => {
                sheet.close();
                sheet.exportRequested(columnId, title);
            }
        }
    }
}
