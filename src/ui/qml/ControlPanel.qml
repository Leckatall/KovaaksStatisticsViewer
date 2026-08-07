import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Frame {
    id: root
    required property var sessionVm
    required property var graphVm
    required property var columnVisibility

    ColumnLayout {
        Text{
            objectName: "renderingLabel"
            text: "Rendering: " + root.sessionVm.getCurrentPerfScenario()
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: scenarioComboBox
                model: root.sessionVm.scenario_list
                enabled: filterByScenarioCheckBox.checked
                editable: true
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
            objectName: "generateProfileButton"
            text: "Generate Profile from current kovaaks dir"
            onClicked: root.sessionVm.generateProfile()
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
            objectName: "loadPerformanceFileButton"
            text: "Load performance File"
            onClicked: selectPerfFileDialog.open()
        }
        Button {
            objectName: "loadLatestPerformanceButton"
            text: "Have Graph Load Latest Performance File"
            onClicked: root.graphVm.fetchData("")
        }

        Label {
            text: "Lines"
            color: "white"
        }
        Repeater {
            model: root.graphVm.plottableColumns

            CheckBox {
                required property int modelData
                objectName: "columnVisibilityCheckBox_" + root.graphVm.columnName(modelData)

                text: root.graphVm.columnName(modelData)
                checked: !!root.columnVisibility[root.graphVm.columnName(modelData).toLowerCase()]
                background: Rectangle {
                    anchors.fill: parent
                    color: root.graphVm.columnColor(modelData)
                    opacity: 0.5
                    radius: 5
                }

                onToggled: root.columnVisibility[root.graphVm.columnName(modelData).toLowerCase()] = checked
            }
        }
    }
}
