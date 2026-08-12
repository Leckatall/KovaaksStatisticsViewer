import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
// DEPRECATED: This component is deprecated and all functionality should be moved from here
Frame {
    id: root
    required property var sessionVm
    required property var graphVm
    required property var columnVisibility

    ColumnLayout {
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
            onClicked: root.graphVm.fetchLatestData()
        }

        Label {
            text: "Lines"
        }
        Repeater {
            model: root.graphVm.plottableColumns

            CheckBox {
                required property int modelData
                objectName: "columnVisibilityCheckBox_" + root.graphVm.columnName(modelData)

                text: root.graphVm.columnName(modelData)
                checked: !!root.columnVisibility[root.graphVm.columnKey(modelData)]
                background: Rectangle {
                    anchors.fill: parent
                    color: root.graphVm.columnColor(modelData)
                    opacity: 0.5
                    radius: 5
                }

                onToggled: root.columnVisibility[root.graphVm.columnKey(modelData)] = checked
            }
        }
    }
}
