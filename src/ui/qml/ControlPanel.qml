import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
            enabled: !root.sessionVm.profileBuildInProgress
            onClicked: root.sessionVm.generateProfile()
        }

        ProgressBar {
            objectName: "profileBuildProgressBar"
            Layout.fillWidth: true
            visible: root.sessionVm.profileBuildInProgress
            // The file count only arrives with the first per-file report; until then
            // there is nothing to show a fraction of.
            indeterminate: value === 0
            value: root.sessionVm.profileBuildProgress
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
