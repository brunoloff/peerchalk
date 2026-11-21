import QtQuick

// Exports the board as a PDF: one page sized to its contents, or a page board page by page.
RailButton {
    id: button

    required property BoardCanvas canvas
    required property string title

    signal reported(string message)

    implicitWidth: 36
    implicitHeight: 36
    text: qsTr("Export as PDF")
    iconPath: Icons.exportFile
    enabled: !flow.busy && button.canvas.contentBounds.width > 0
    onClicked: flow.start(button.title, "", 0, 0)

    PdfExportFlow {
        id: flow

        canvas: button.canvas
        onReported: message => button.reported(message)
    }
}
