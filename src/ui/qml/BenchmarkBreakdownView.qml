import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var breakdown
    property var trackingVm
    property bool wide: true

    implicitWidth: rootColumn.implicitWidth
    implicitHeight: rootColumn.implicitHeight

    Column {
        id: rootColumn
        width: root.width
        spacing: 4

        Repeater {
            model: root.breakdown ? root.breakdown.nodes : []

            delegate: BenchmarkBreakdownNode {
                required property var modelData
                width: rootColumn.width
                node: modelData
                trackingVm: root.trackingVm
                wide: root.wide
                depth: 0
            }
        }
    }
}
