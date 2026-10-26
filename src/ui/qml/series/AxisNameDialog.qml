import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer
import "axisNames.js" as AxisNames

Dialog {
    id: root

    required property var settingsVm
    property string assignLineId: ""
    readonly property alias nameField: nameInput
    readonly property alias errorLabel: errorText

    signal created(string axisId)

    objectName: "newAxisDialog"
    title: qsTr("New axis")
    modal: true
    anchors.centerIn: Overlay.overlay

    function openFor(lineId) {
        assignLineId = lineId || ""
        nameInput.text = ""
        errorText.text = ""
        open()
        nameInput.forceActiveFocus()
    }

    // Closing is driven here rather than by an AcceptRole button, which would close the dialog on a rejected name.
    function tryCreate() {
        const result = settingsVm.createAxis(nameInput.text)
        if (!result || !result.succeeded) {
            errorText.text = AxisNames.message(result ? result.issue : "")
            return
        }
        if (assignLineId !== "")
            settingsVm.updateSeriesAxis(assignLineId, result.createdAxisId)
        created(result.createdAxisId)
        close()
    }

    contentItem: ColumnLayout {
        spacing: 6

        Label {
            objectName: "newAxisNameLabel"
            text: qsTr("Axis name")
        }

        TextField {
            id: nameInput
            objectName: "newAxisNameField"
            Layout.fillWidth: true
            Layout.minimumWidth: 260
            Accessible.name: qsTr("Axis name")
            Accessible.description: errorText.text
            onAccepted: root.tryCreate()
        }

        Label {
            id: errorText
            objectName: "newAxisErrorLabel"
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: text !== ""
            color: Theme.errorText
            wrapMode: Text.WordWrap
        }
    }

    footer: DialogButtonBox {
        ActionButton {
            objectName: "newAxisCreateButton"
            text: qsTr("Create")
            highlighted: true
            onClicked: root.tryCreate()
        }
        ActionButton {
            objectName: "newAxisCancelButton"
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
