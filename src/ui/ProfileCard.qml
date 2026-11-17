pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The identity screen's card about you: name, colour, fingerprint and contact code.
Panel {
    id: profile

    signal showContactCode()
    signal linkDeviceRequested()

    implicitHeight: column.implicitHeight + 48

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        SectionLabel {
            text: qsTr("You")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Avatar {
                name: Library.displayName
                size: 56
                color: Theme.personColors[Library.colorIndex]
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Display name")
                    font.pixelSize: 12
                    color: Theme.muted
                }

                TextField {
                    Layout.fillWidth: true
                    text: Library.displayName
                    Accessible.name: qsTr("Display name")
                    onEditingFinished: {
                        if (text.trim().length > 0 && text.trim() !== Library.displayName)
                            Library.displayName = text;
                    }
                }
            }
        }

        Label {
            text: qsTr("Cursor colour")
            font.pixelSize: 12
            color: Theme.muted
        }

        Row {
            spacing: 10

            Repeater {
                model: Theme.personColors

                delegate: AbstractButton {
                    id: swatch

                    required property string modelData
                    required property int index
                    readonly property bool current: Library.colorIndex === index

                    implicitWidth: 32
                    implicitHeight: 32
                    Accessible.name: qsTr("Colour %1").arg(index + 1)
                    Accessible.checked: current
                    onClicked: Library.colorIndex = index

                    background: Rectangle {
                        radius: 16
                        color: swatch.modelData
                        border.width: swatch.current ? 3 : 0
                        border.color: Theme.surface

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: width / 2
                            color: "transparent"
                            border.width: swatch.current ? 2 : 0
                            border.color: swatch.modelData
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Your key fingerprint — friends compare this to verify you")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.muted
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: fingerprint.implicitHeight + 24
            radius: Theme.controlRadius
            color: Theme.background

            Label {
                id: fingerprint

                anchors.fill: parent
                anchors.margins: 12
                text: Library.fingerprint
                wrapMode: Text.WordWrap
                font.family: Theme.monoFont
                font.pixelSize: 15
                font.letterSpacing: 1
                lineHeight: 1.4
                color: Theme.ink
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            PaperButton {
                Layout.fillWidth: true
                text: qsTr("Copy my contact code")
                onClicked: Library.copyToClipboard(Library.contactCode)
            }

            PaperButton {
                text: qsTr("Show")
                onClicked: profile.showContactCode()
            }
        }

        SectionLabel {
            text: qsTr("Devices")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: Theme.ink
                text: qsTr("This device: %1").arg(Library.deviceName)
            }

            PaperButton {
                text: qsTr("Link another device")
                onClicked: profile.linkDeviceRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Your secret key stays on your devices. Losing all of them means friends must verify a new identity.")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            lineHeight: 1.3
            color: Theme.muted
        }
    }
}
