import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The connection log in the board's network pane: hidden until asked for, with copy and
// save, for sending to whoever helps (Settings → Troubleshooting shows the same log).
ColumnLayout {
    id: section

    readonly property bool inBrowser: Qt.platform.os === "wasm"
    // Untyped: NativeSaveFile does not exist in the browser build.
    readonly property var saveDialog: saveDialogLoader.item
    property bool shown: false
    property string notice
    readonly property int shownLines: 200

    function refresh() {
        if (shown)
            logArea.text = Network.logText(section.shownLines);
    }

    spacing: 8
    onShownChanged: refresh()

    Connections {
        target: Network

        function onLogChanged() {
            section.refresh();
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 12
        color: Theme.muted
        text: qsTr("If it still does not work, save the log and send it to whoever helps, ideally "
            + "from both sides. It holds no passwords or keys.")
    }

    Flow {
        Layout.fillWidth: true
        spacing: 6

        PaperButton {
            text: section.shown ? qsTr("Hide log") : qsTr("Show log")
            onClicked: section.shown = !section.shown
        }

        PaperButton {
            text: section.inBrowser ? qsTr("Download") : qsTr("Save…")
            onClicked: {
                if (section.inBrowser)
                    section.notice = Network.downloadLog();
                else
                    section.saveDialog.save(Network.logFileName());
            }
        }

        PaperButton {
            text: qsTr("Copy")
            onClicked: {
                Network.copyLog();
                section.notice = qsTr("Copied.");
            }
        }
    }

    Label {
        visible: section.notice.length > 0
        text: section.notice
        font.pixelSize: 12
        color: Theme.muted
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 260
        visible: section.shown

        TextArea {
            id: logArea

            objectName: "boardConnectionLog"
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            font.family: Theme.monoFont
            font.pixelSize: 11
            Accessible.name: qsTr("Connection log")
            onTextChanged: cursorPosition = length

            background: Rectangle {
                color: Theme.background
                border.color: Theme.border
                radius: Theme.controlRadius
            }
        }
    }

    Loader {
        id: saveDialogLoader

        source: section.inBrowser ? "" : "NativeSaveFile.qml"
        onLoaded: {
            item.nameFilters = [qsTr("Text files (*.txt)")];
            item.defaultSuffix = "txt";
        }
    }

    Connections {
        target: section.saveDialog
        ignoreUnknownSignals: true

        function onChosen(file: url) {
            const error = Network.saveLog(file);
            section.notice = error.length > 0 ? error : qsTr("Saved.");
        }
    }
}
