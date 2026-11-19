import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → Troubleshooting: what happened while finding and connecting to other devices,
// to send to whoever helps when a connection does not work.
Item {
    id: pane

    readonly property bool inBrowser: Qt.platform.os === "wasm"
    // Untyped: NativeSaveFile does not exist in the browser build.
    readonly property var saveDialog: saveDialogLoader.item
    property string notice
    // The page shows the most recent lines; copying and saving include everything.
    readonly property int shownLines: 400

    function refresh() {
        if (!visible)
            return;
        const atEnd = logView.atYEnd;
        logArea.text = Network.logText(pane.shownLines);
        if (atEnd)
            logArea.cursorPosition = logArea.length;
    }

    onVisibleChanged: refresh()

    Connections {
        target: Network

        function onLogChanged() {
            pane.refresh();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("If you cannot connect to someone, copy or save this log and send it to whoever "
                + "is helping, ideally from both of you. It lists the servers used, shortened "
                + "device ids, names and what each step reported. It holds no passwords or keys, "
                + "and it is kept only until PeerChalk closes.")
            wrapMode: Text.Wrap
            color: Theme.muted
        }

        RowLayout {
            spacing: 8

            PaperButton {
                text: qsTr("Copy log")
                onClicked: {
                    Network.copyLog();
                    pane.notice = qsTr("Copied.");
                }
            }

            PaperButton {
                text: qsTr("Save log…")
                onClicked: {
                    if (pane.inBrowser)
                        pane.notice = Network.downloadLog();
                    else
                        pane.saveDialog.save(Network.logFileName());
                }
            }

            PaperButton {
                text: qsTr("Clear")
                onClicked: {
                    Network.clearLog();
                    pane.notice = "";
                }
            }

            Label {
                Layout.leftMargin: 8
                text: pane.notice
                color: Theme.muted
            }
        }

        ScrollView {
            id: logView

            readonly property bool atYEnd: (contentItem as Flickable)?.atYEnd ?? true

            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: logArea

                objectName: "troubleshootingLog"
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font.family: Theme.monoFont
                font.pixelSize: 12
                Accessible.name: qsTr("Connection log")

                background: Rectangle {
                    color: Theme.surface
                    border.color: Theme.border
                    radius: Theme.controlRadius
                }
            }
        }
    }

    Loader {
        id: saveDialogLoader

        source: pane.inBrowser ? "" : "NativeSaveFile.qml"
        onLoaded: {
            item.nameFilters = [qsTr("Text files (*.txt)")];
            item.defaultSuffix = "txt";
        }
    }

    Connections {
        target: pane.saveDialog
        ignoreUnknownSignals: true

        function onChosen(file: url) {
            const error = Network.saveLog(file);
            pane.notice = error.length > 0 ? error : qsTr("Saved.");
        }
    }
}
