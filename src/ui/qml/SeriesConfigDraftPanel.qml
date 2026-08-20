import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root

    required property var settingsVm

    objectName: "seriesConfigDraftPanel"
    spacing: 8

    Component.onCompleted: settingsVm.beginSeriesDraft()

    ColumnLayout {
        Repeater {
            model: root.settingsVm.allSeriesConfigs

            RowLayout {
                id: lineRow

                required property var modelData

                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    border.color: root.palette.mid
                    border.width: 1
                    color: lineRow.modelData.color
                    height: 16
                    radius: 4
                    width: 16
                }
                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: lineRow.modelData.name
                }
                Item {
                    Layout.fillWidth: true
                }
                Switch {
                    checked: lineRow.modelData.enabled
                    objectName: "seriesEnabledSwitch_" + lineRow.modelData.id

                    onToggled: root.settingsVm.setSeriesEnabled(lineRow.modelData.id, checked)
                }
            }
        }
        Label {
            id: seriesDraftErrorLabel

            Layout.fillWidth: true
            color: "#E53935"
            objectName: "seriesDraftErrorLabel"
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                enabled: root.settingsVm.pendingChanges
                objectName: "saveGraphLinesButton"
                text: "Save Graph Lines"

                onClicked: {
                    const result = root.settingsVm.commitSeriesDraft();
                    seriesDraftErrorLabel.text = (result && result.succeeded === false) ? "Couldn't save graph line changes. Your edits are still here — try again." : "";
                }
            }
            Button {
                enabled: root.settingsVm.pendingChanges
                objectName: "discardGraphLineChangesButton"
                text: "Discard Graph Line Changes"

                onClicked: root.settingsVm.discardSeriesDraft()
            }
        }
    }
}

