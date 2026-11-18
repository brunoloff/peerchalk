import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Links to the privacy note and license notices on the app's website.
RowLayout {
    id: links

    function open(page: string) {
        const site = Accounts.siteUrl.endsWith("/") ? Accounts.siteUrl : Accounts.siteUrl + "/";
        Qt.openUrlExternally(site + page);
    }

    spacing: 8

    PaperButton {
        flat: true
        text: qsTr("Privacy")
        onClicked: links.open("privacy.html")
    }

    PaperButton {
        flat: true
        text: qsTr("Licenses")
        onClicked: links.open("licenses/")
    }
}
