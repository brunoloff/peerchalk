import QtQuick
import QtQuick.Controls

// This build's version ("0.2.3", or "0.2.3+7" seven commits past it), small and pressed into
// the paper, for beside the name. Place it in a layout aligned to the name's baseline.
Label {
    property int size: 11

    text: Launcher.build
    font.family: Theme.headingFont
    font.pixelSize: size
    font.weight: Font.Medium
    font.letterSpacing: size * 0.055
    color: Theme.engraved
    style: Text.Sunken
    styleColor: Theme.engravedEdge
    Accessible.name: qsTr("Version %1").arg(Launcher.build)
}
