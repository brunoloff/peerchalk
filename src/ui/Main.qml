pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PeerChalk

ApplicationWindow {
    id: window

    width: 1024
    height: 720
    visible: true
    title: qsTr("PeerChalk")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8

            Repeater {
                model: ["#1d1d1f", "#d93025", "#1a73e8", "#188038"]

                delegate: RoundButton {
                    id: swatch

                    required property string modelData

                    checkable: true
                    checked: Qt.colorEqual(canvas.penColor, swatch.modelData)
                    Accessible.name: qsTr("Pen colour %1").arg(swatch.modelData)
                    onClicked: canvas.penColor = swatch.modelData

                    contentItem: Rectangle {
                        implicitWidth: 18
                        implicitHeight: 18
                        radius: 9
                        color: swatch.modelData
                    }
                }
            }

            Label {
                text: qsTr("Width")
            }

            Slider {
                from: 1
                to: 24
                value: canvas.penWidth
                onMoved: canvas.penWidth = value
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Clear")
                enabled: canvas.strokeCount > 0
                onClicked: canvas.clear()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "white"

        BoardCanvas {
            id: canvas

            anchors.fill: parent
        }
    }
}
