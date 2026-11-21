import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One person in the members dialog: their role, and the role controls `myRole` allows.
// A Repeater delegate over Library.projectPeople / boardPeople entries.
RowLayout {
    id: row

    required property var modelData
    required property string myRole
    readonly property string role: modelData.role
    readonly property bool isAdmin: myRole === "admin"
    // Owners manage plain members only; the admin manages everyone but themself.
    readonly property bool manageable: !modelData.you && role !== "admin"
        && (isAdmin || (myRole === "owner" && (role === "member" || role === "none")))
    readonly property var roleLabels: ({
        "admin": qsTr("Admin"),
        "owner": qsTr("Owner"),
        "member": qsTr("Member"),
        "none": qsTr("No access")
    })

    signal roleRequested(string key, string role)
    signal adminRequested(string key)

    Layout.fillWidth: true
    spacing: 12

    Avatar {
        name: row.modelData.name
        size: 34
        color: Theme.personColors[Format.keyColorIndex(row.modelData.key, Theme.personColors.length)]
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.ink
            font.weight: Font.DemiBold
            text: row.modelData.name + (row.modelData.you ? qsTr(" (you)") : "")
        }

        Label {
            text: row.roleLabels[row.role] + " · " + row.modelData.fingerprint.slice(0, 14) + " …"
            font.pixelSize: 12
            color: Theme.muted
        }
    }

    PaperButton {
        visible: row.manageable && row.role === "none"
        text: qsTr("Add")
        onClicked: row.roleRequested(row.modelData.key, "member")
    }

    RailButton {
        visible: row.manageable && row.role !== "none"
        implicitWidth: 36
        implicitHeight: 36
        text: qsTr("Change %1's access").arg(row.modelData.name)
        iconPath: Icons.more
        onClicked: menu.popup()

        PaperMenu {
            id: menu

            PaperMenuItem {
                visible: row.isAdmin && row.role === "member"
                height: visible ? implicitHeight : 0
                text: qsTr("Make owner")
                onTriggered: row.roleRequested(row.modelData.key, "owner")
            }

            PaperMenuItem {
                visible: row.isAdmin && row.role === "owner"
                height: visible ? implicitHeight : 0
                text: qsTr("Make member")
                onTriggered: row.roleRequested(row.modelData.key, "member")
            }

            PaperMenuItem {
                visible: row.isAdmin
                height: visible ? implicitHeight : 0
                text: qsTr("Make admin (you become an owner)")
                onTriggered: row.adminRequested(row.modelData.key)
            }

            PaperMenuItem {
                text: qsTr("Remove access")
                destructive: true
                onTriggered: row.roleRequested(row.modelData.key, "none")
            }
        }
    }
}
