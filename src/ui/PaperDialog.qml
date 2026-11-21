import QtQuick
import QtQuick.Controls

// A dialog in the Paper style: a white card with large rounded corners and a soft shadow,
// its title in the heading font. Use a PaperButtonBox as its footer.
Dialog {
    id: dialog

    parent: Overlay.overlay
    anchors.centerIn: parent
    padding: 24
    topPadding: 8
    modal: true

    background: Item {
        SoftShadow {
            anchors.fill: parent
            radius: 18
            spread: 36
            strength: 0.22
        }

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Theme.surface
        }
    }

    header: Label {
        visible: dialog.title.length > 0
        text: dialog.title
        topPadding: 24
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 8
        wrapMode: Text.WordWrap
        font.family: Theme.headingFont
        font.pixelSize: 20
        font.weight: Font.DemiBold
        color: Theme.ink
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0.11, 0.106, 0.098, 0.32)
    }
}
