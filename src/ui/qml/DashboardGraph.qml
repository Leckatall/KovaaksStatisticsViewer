import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import QtGraphs

Frame {
    id: root
    required property var graphVm

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.horizontalStretchFactor: 3

    background: Rectangle {
        radius: 12
        color: "#1E1E1E"
        border.color: "#2A2A2A"
    }

    // Score is drawn as the primary, labeled Y axis; every other plottable
    // column gets its own invisible axis scaled to its own range so series
    // with very different magnitudes (e.g. Accuracy 0-1 vs Score) can share
    // the same chart without distorting one another.
    readonly property int primaryColumn: GraphViewModel.Score
    readonly property int lineCount: GraphViewModel.ColumnCount - 1

    GraphsView {
        id: graphsView
        anchors.fill: parent
        anchors.margins: 1
        axisX: ValueAxis {
            min: root.graphVm.axisBounds[GraphViewModel.Time].x
            max: root.graphVm.axisBounds[GraphViewModel.Time].y
        }

        Instantiator {
            model: root.lineCount

            LineFromModel {
                id: line
                required property int index
                readonly property int columnId: index + 1

                line_model: root.graphVm
                xIndex: GraphViewModel.Time
                yIndex: columnId
                color: root.graphVm.columnColor(columnId)
                width: 3
                visible: root.graphVm.columnVisibility[columnId]
                pointDelegate: GraphHoverPointDelegate {}

                axisY: ValueAxis {
                    min: root.graphVm.axisBounds[line.columnId].x
                    max: root.graphVm.axisBounds[line.columnId].y
                    visible: line.columnId === root.primaryColumn
                    labelsVisible: line.columnId === root.primaryColumn
                    gridVisible: line.columnId === root.primaryColumn
                    lineVisible: line.columnId === root.primaryColumn
                    subTickCount: 4
                }
            }

            onObjectAdded: (index, object) => graphsView.addSeries(object)
            onObjectRemoved: (index, object) => graphsView.removeSeries(object)
        }
    }
}
