import QtQuick
import QtQuick.Controls

// PeerChalk's version, small and pressed into the paper, for beside the name. Place it in a
// layout aligned to the name's baseline.
Label {
    property int size: 11

    text: Launcher.version
    font.family: Theme.headingFont
    font.pixelSize: size
    font.weight: Font.Medium
    font.letterSpacing: size * 0.055
    color: Theme.engraved
    style: Text.Sunken
    styleColor: Theme.engravedEdge
    Accessible.name: qsTr("Version %1").arg(Launcher.version)
}
