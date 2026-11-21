import QtQuick
import QtQuick.Controls

// A menu in the Paper style: a white card with rounded corners and a soft shadow, holding
// PaperMenuItems. Its submenus are PaperMenus too.
Menu {
    id: menu

    // As a submenu: what it is set to, shown on its row in the parent menu.
    property string value

    implicitWidth: 230
    padding: 6
    margins: 8
    delegate: PaperMenuItem {}

    background: Item {
        implicitWidth: menu.implicitWidth
        implicitHeight: 40

        SoftShadow {
            anchors.fill: parent
            radius: 12
            spread: 14
        }

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: Theme.surface
            border.color: Theme.border
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 90
        }
    }
}
