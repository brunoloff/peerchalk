import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Your identity and the people you know.
Item {
    id: page

    signal back()

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("Identity & contacts")
            onBack: page.back()
        }

        ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            Flow {
                width: scroll.availableWidth
                padding: 28
                spacing: 28

                ProfileCard {
                    width: Math.min(420, scroll.availableWidth - 56)
                    onShowContactCode: contactCodeDialog.open()
                }

                PeopleCard {
                    width: scroll.availableWidth >= 1000 ? scroll.availableWidth - 420 - 84 : Math.min(scroll.availableWidth - 56, 720)
                    onAddPersonRequested: addPersonDialog.ask("")
                    onRemoveRequested: (key, name) => {
                        removePersonDialog.personKey = key;
                        removePersonDialog.message = qsTr("Remove %1 from your known people? You can add them again with their contact code.").arg(name);
                        removePersonDialog.open();
                    }
                }
            }
        }
    }

    CodeDialog {
        id: contactCodeDialog

        title: qsTr("Your contact code")
        code: Library.contactCode
        explanation: qsTr("Send this to people so they can add you. It holds only your public key and name. Then compare fingerprints to verify each other.")
    }

    TextInputDialog {
        id: addPersonDialog

        title: qsTr("Add a person")
        label: qsTr("Paste the contact code they sent you.")
        placeholder: "peerchalk-contact:1:…"
        acceptText: qsTr("Add")
        onSubmitted: text => Library.addContact(text)
    }

    ConfirmDialog {
        id: removePersonDialog

        property string personKey

        title: qsTr("Remove person")
        confirmText: qsTr("Remove")
        onConfirmed: Library.removeContact(personKey)
    }
}
