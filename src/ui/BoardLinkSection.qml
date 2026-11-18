import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// For a board's owners: the link that lets anyone who has it join as a member.
ColumnLayout {
    id: section

    property string boardId
    property string link

    function refresh() {
        link = boardId.length > 0 ? Library.boardLink(boardId, Accounts.siteUrl) : "";
    }

    spacing: 8
    onBoardIdChanged: refresh()

    SectionLabel {
        text: qsTr("Board link")
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.muted
        text: qsTr("Anyone with this link becomes a member while an owner is online. Members can draw only while an owner is connected.")
    }

    TextField {
        objectName: "boardLinkField"
        Layout.fillWidth: true
        visible: section.link.length > 0
        readOnly: true
        text: section.link
        Accessible.name: qsTr("Board link")
    }

    RowLayout {
        spacing: 8

        PaperButton {
            objectName: "createBoardLink"
            primary: section.link.length === 0
            text: section.link.length > 0 ? qsTr("Replace link") : qsTr("Create link")
            onClicked: {
                section.link = Library.createBoardLink(section.boardId, Accounts.siteUrl);
                if (section.link.length > 0)
                    Library.copyToClipboard(section.link);
            }
        }

        PaperButton {
            visible: section.link.length > 0
            text: qsTr("Copy link")
            onClicked: Library.copyToClipboard(section.link)
        }

        PaperButton {
            visible: section.link.length > 0
            destructive: true
            text: qsTr("Turn off")
            onClicked: {
                Library.disableBoardLink(section.boardId);
                section.refresh();
            }
        }
    }
}
