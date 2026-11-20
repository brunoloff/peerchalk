import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What PeerChalk is, on the account screen: a note from its author, four points about how
// it works, and links to the app for computers and to feature requests. AppImages can add
// themselves to the app launcher, and installed apps can look for a newer version.
ColumnLayout {
    id: about

    property int factColumns: 2
    property int noteSize: 16
    readonly property bool inBrowser: Qt.platform.os === "wasm"
    readonly property string releasesUrl: "https://github.com/brunoloff/peerchalk/releases/latest"
    readonly property string issuesUrl: "https://github.com/brunoloff/peerchalk/issues/new"
    readonly property string system: Accounts.visitorSystem
    readonly property string otherSystems: ["Linux", "Windows", "macOS"].filter(name => name !== about.system).join(qsTr(" and "))

    spacing: 24

    // The browser build's Qt has no emoji font, so it leaves the emoji out.
    Label {
        Layout.fillWidth: true
        text: qsTr("I had Codex & Claude build this because I don't like the other boards. I'm amazed that with 50 prompts over two days it's already better than the rest.\nThe end of math... the end of software... that's AI for you, yeay!") + (about.inBrowser ? "" : " 😬")
        wrapMode: Text.WordWrap
        font.pixelSize: about.noteSize
        lineHeight: 1.25
        color: Theme.muted
    }

    GridLayout {
        Layout.fillWidth: true
        columns: about.factColumns
        columnSpacing: 32
        rowSpacing: 18

        WelcomeFact {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            icon: Icons.peers
            title: qsTr("No server, peer to peer")
            detail: qsTr("Peers find each other through OpenDHT, which works much like BitTorrent.")
        }

        WelcomeFact {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            icon: Icons.browser
            title: qsTr("Works in the browser")
            detail: qsTr("Peers talk over WebRTC, so browsers and the app share the same boards.")
        }

        WelcomeFact {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            icon: Icons.bolt
            title: qsTr("Fast")
            detail: qsTr("Written in C++ and compiled to WebAssembly for the browser.")
        }

        WelcomeFact {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            icon: Icons.relay
            title: qsTr("Behind a firewall?")
            detail: qsTr("On a strict NAT or a crappy phone connection, turn on a TURN relay.")
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.border
    }

    Flow {
        Layout.fillWidth: true
        Layout.topMargin: -4
        spacing: 10

        PaperButton {
            visible: about.inBrowser
            strong: true
            implicitHeight: 44
            text: about.system.length > 0 ? qsTr("Download for %1").arg(about.system) : qsTr("Get the app")
            onClicked: Qt.openUrlExternally(about.releasesUrl)
        }

        PaperButton {
            visible: Launcher.available && Launcher.state !== Launcher.State.Current
            strong: true
            implicitHeight: 44
            text: Launcher.state === Launcher.State.Missing ? qsTr("Add to app launcher")
                : qsTr("Update the app launcher")
            onClicked: Launcher.install()
        }

        PaperButton {
            visible: !about.inBrowser && Updates.status !== Updates.Status.Checking
                && Updates.status !== Updates.Status.Downloading
            implicitHeight: 44
            text: qsTr("Check for updates")
            onClicked: Updates.check()
        }

        PaperButton {
            implicitHeight: 44
            text: qsTr("Request a feature")
            onClicked: Qt.openUrlExternally(about.issuesUrl)
        }

        SiteLinks {
            height: 44
        }
    }

    UpdateStatus {
        Layout.fillWidth: true
        Layout.topMargin: -12
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: -12
        visible: about.inBrowser
        text: about.system.length > 0
            ? qsTr("The app for your computer starts faster than this browser version and keeps boards outside the browser's storage. Also for <a href=\"%1\">%2</a>.").arg(about.releasesUrl).arg(about.otherSystems)
            : qsTr("The app for computers starts faster than this browser version and keeps boards outside the browser's storage. It runs on Linux, Windows and macOS.")
        textFormat: Text.StyledText
        linkColor: Theme.accent
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        color: Theme.muted
        onLinkActivated: link => Qt.openUrlExternally(link)
    }
}
