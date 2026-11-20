import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// PeerChalk's logo on the account screen: the tilted icon beside the name, under a joke
// scribbled above it with an arrow to the icon. The joke keeps its space while hidden, so
// showing it moves nothing.
ColumnLayout {
    id: logo

    property int iconSize: 176
    property int titleSize: 60
    property int noteSize: 13
    property bool showSubtitle: true
    property int subtitleSize: 23
    // The joke shows on the first run; otherwise only while the logo is hovered or after a
    // tap on it, which is how phones show it.
    property bool showJoke: false
    property bool tapped: false
    readonly property bool jokeShown: showJoke || tapped || hover.hovered

    spacing: 2

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: logo.tapped = !logo.tapped
    }

    RowLayout {
        Layout.leftMargin: -logo.noteSize / 2
        spacing: 4
        rotation: -5
        transformOrigin: Item.BottomLeft
        opacity: logo.jokeShown ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        Label {
            text: qsTr("Yes, the name is stupid,\nbut the icon is cute.\nAlso, fuck you.")
            font.family: Theme.headingFont
            font.pixelSize: logo.noteSize
            font.weight: Font.DemiBold
            lineHeight: 1.1
            color: Theme.muted
        }

        Icon {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: -logo.noteSize * 0.8
            implicitWidth: logo.noteSize * 3
            implicitHeight: logo.noteSize * 3
            path: Icons.scribbleArrow
            color: Theme.muted
        }
    }

    RowLayout {
        spacing: Math.round(logo.iconSize * 0.14)

        Image {
            Layout.preferredWidth: logo.iconSize
            Layout.preferredHeight: logo.iconSize
            source: "qrc:/PeerChalk/peerchalk-logo.png"
            sourceSize: Qt.size(512, 512)
            fillMode: Image.PreserveAspectFit
            mipmap: true
            rotation: -3
            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("PeerChalk icon: two people drawing on a blackboard")
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 14

            Label {
                text: "PeerChalk"
                font.family: Theme.headingFont
                font.pixelSize: logo.titleSize
                font.weight: Font.Bold
                font.letterSpacing: -logo.titleSize * 0.02
                color: Theme.ink
            }

            Label {
                visible: logo.showSubtitle
                text: qsTr("A decentralized, shared whiteboard\nfor mathematicians & their friends")
                font.family: Theme.headingFont
                font.pixelSize: logo.subtitleSize
                font.weight: Font.DemiBold
                lineHeight: 1.1
                color: Theme.ink
                Accessible.role: Accessible.Heading
            }
        }
    }
}
