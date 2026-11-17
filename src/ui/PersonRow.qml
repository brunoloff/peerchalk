import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One known person; a Repeater delegate over Library.contacts.
ItemDelegate {
    id: row

    required property string key
    required property string name
    required property string fingerprint
    required property string verification
    property bool selected: false
    readonly property bool verified: verification !== "none"

    Accessible.name: qsTr("%1, %2").arg(name).arg(verified ? qsTr("verified") : qsTr("not verified"))

    background: Rectangle {
        radius: 12
        color: row.selected ? Theme.accentSoft : !row.verified ? Theme.warningSoft : row.hovered ? Theme.hover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 14

        Avatar {
            name: row.name
            size: 40
            color: Theme.personColors[Format.keyColorIndex(row.key, Theme.personColors.length)]
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: row.name
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Label {
                Layout.fillWidth: true
                text: row.fingerprint.slice(0, 19) + " …"
                elide: Text.ElideRight
                font.family: Theme.monoFont
                font.pixelSize: 12
                color: Theme.muted
            }
        }

        Rectangle {
            implicitWidth: badge.implicitWidth + 20
            implicitHeight: 26
            radius: 13
            color: row.verified ? Theme.successSoft : "transparent"
            border.width: row.verified ? 0 : 1
            border.color: Theme.border

            RowLayout {
                id: badge

                anchors.centerIn: parent
                spacing: 4

                Icon {
                    visible: row.verified
                    implicitWidth: 14
                    implicitHeight: 14
                    path: Icons.check
                    color: Theme.success
                    lineWidth: 2.6
                }

                Label {
                    text: row.verified ? qsTr("Verified") : qsTr("Not verified")
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: row.verified ? Theme.success : Theme.muted
                }
            }
        }
    }
}
