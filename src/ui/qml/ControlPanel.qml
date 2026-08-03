import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Frame {
    id: root
    required property var sessionVm
    required property var graphVm

    ColumnLayout {
        RowLayout {
            ComboBox {
                id: scenarioComboBox
                model: root.sessionVm.scenario_list
                enabled: filterByScenarioCheckBox.checked
                // editable: true
                Layout.fillWidth: true
            }
            CheckBox {
                id: filterByScenarioCheckBox
                text: "Filter by Scenario"
                checked: false
            }
        }
        ComboBox {
            id: displayScenarioModeComboBox
        }
        Button {
            text: "Generate Profile from current kovaaks dir"
            onClicked: root.sessionVm.generateProfile()
        }
        Button {
            text: "update comboBox"
            onClicked: {
                scenarioComboBox.model = root.sessionVm.scenario_list
            }
        }

        FileDialog {
            id: selectPerfFileDialog
            title: "Load Performance"
            nameFilters: ["Performance Files (*.perf)"]
            onAccepted: {
                root.graphVm.fetchData(selectPerfFileDialog.selectedFiles[0])
            }
        }
        Button {
            text: "Load performance File"
            onClicked: selectPerfFileDialog.open()
        }
        Button {
            text: "Have Graph Load Latest Performance File"
            onClicked: root.graphVm.fetchData("")
        }
    }
}
