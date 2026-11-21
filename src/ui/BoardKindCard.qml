import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A kind of board to pick in the new-board dialog: a small picture of it, its name and what
// it is for.
AbstractButton {
    id: card

    property string summary
    property bool pages: false // pictures a page board instead of an infinite one

    checkable: true
    hoverEnabled: true
    padding: 14
    implicitHeight: content.implicitHeight + 28
    Accessible.role: Accessible.RadioButton
    Accessible.description: card.summary

    contentItem: RowLayout {
        id: content

        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: 64
            implicitHeight: 64
            radius: 10
            color: card.pages ? "#ECEAE4" : Theme.boardBackground
            border.color: Theme.border
            clip: true

            Canvas { // the infinite board: dots and a stroke
                anchors.fill: parent
                visible: !card.pages
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = "#C9C5BC";
                    for (let y = 4.5; y < height; y += 9)
                        for (let x = 4.5; x < width; x += 9)
                            ctx.fillRect(x - 0.6, y - 0.6, 1.2, 1.2);
                    ctx.strokeStyle = Theme.ink;
                    ctx.lineWidth = 2;
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    ctx.moveTo(12, 40);
                    ctx.bezierCurveTo(20, 26, 28, 44, 36, 32);
                    ctx.bezierCurveTo(44, 20, 46, 26, 52, 28);
                    ctx.stroke();
                }
            }

            Repeater { // the page board: four sheets in two columns
                model: card.pages ? 4 : 0

                Rectangle {
                    required property int index

                    x: index % 2 === 0 ? 9 : 35
                    y: index < 2 ? 9 : 38
                    width: 20
                    height: 26
                    color: "white"
                    border.color: Qt.rgba(0, 0, 0, 0.08)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: card.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Label {
                Layout.fillWidth: true
                text: card.summary
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                lineHeight: 1.2
                color: Theme.muted
            }
        }
    }

    background: Rectangle {
        radius: 14
        color: card.checked ? "#F6F8FE" : card.hovered ? Theme.hover : Theme.surface
        border.color: card.checked ? Theme.accent : Theme.border
        border.width: card.checked || card.visualFocus ? 2 : 1
    }
}
