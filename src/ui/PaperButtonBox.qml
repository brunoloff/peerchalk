import QtQuick
import QtQuick.Controls

// The buttons along the bottom of a PaperDialog, right-aligned, on the dialog's own white,
// with Cancel before the button that does it, as in the designs, on every system.
DialogButtonBox {
    buttonLayout: DialogButtonBox.MacLayout
    padding: 24
    topPadding: 8
    spacing: 10
    alignment: Qt.AlignRight
    background: Item {}
}
