pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings → People: the list of known people, with verification of the selected one.
Panel {
    id: people

    property string selectedKey
    // The selection while the list is rebuilt; rows that match it reselect themselves.
    property string pendingKey
    property string selectedName
    property string selectedFingerprint
    property string selectedVerification

    signal addPersonRequested()
    signal removeRequested(string key, string name)

    function select(key: string, name: string, fingerprint: string, verification: string) {
        selectedKey = key;
        selectedName = name;
        selectedFingerprint = fingerprint;
        selectedVerification = verification;
    }

    implicitHeight: column.implicitHeight + 48

    // When the list is rebuilt, the selected row (if the person is still known) selects
    // itself again with its current details; otherwise the selection is cleared.
    Connections {
        target: Library.contacts

        function onModelAboutToBeReset() {
            people.pendingKey = people.selectedKey;
            people.selectedKey = "";
        }
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            SectionLabel {
                Layout.fillWidth: true
                text: qsTr("Known people")
            }

            PaperButton {
                text: qsTr("Add person")
                primary: true
                onClicked: people.addPersonRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: Library.contacts.count === 0
            text: qsTr("No one yet. Ask friends for their contact code and add them here; they can add you with yours.")
            wrapMode: Text.WordWrap
            color: Theme.muted
        }

        Repeater {
            model: Library.contacts

            delegate: PersonRow {
                id: person

                Layout.fillWidth: true
                selected: people.selectedKey === person.key
                onClicked: people.select(person.key, person.name, person.fingerprint, person.verification)
                Component.onCompleted: {
                    if (person.key === people.pendingKey)
                        people.select(person.key, person.name, person.fingerprint, person.verification);
                }
            }
        }

        VerifyPanel {
            Layout.fillWidth: true
            visible: people.selectedKey.length > 0
            personKey: people.selectedKey
            personName: people.selectedName
            fingerprint: people.selectedFingerprint
            verification: people.selectedVerification
            onRemoveRequested: people.removeRequested(people.selectedKey, people.selectedName)
        }
    }
}
