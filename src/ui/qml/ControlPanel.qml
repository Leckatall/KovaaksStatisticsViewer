import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// DEPRECATED: This component is deprecated and all functionality should be moved from here
Frame {
    id: root
    required property var graphVm
    required property var columnVisibility

    ColumnLayout {
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
