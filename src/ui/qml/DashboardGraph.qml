import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import QtGraphs

Frame {
    id: root
    required property var graphVm
    required property var columnVisibility

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.horizontalStretchFactor: 3

    background: Rectangle {
        radius: 12
        color: "#1E1E1E"
        border.color: "#2A2A2A"
    }

    // Score is drawn as the primary, labelled Y axis; every other plottable
    // column gets its own invisible axis scaled to its own range so series
    // with very different magnitudes (e.g. Accuracy 0-1 vs Score) can share
    // the same chart without distorting one another.
    //
    // These read off the graphVm instance rather than the bare
    // "GraphViewModel.Score"-style static enum type name: referencing the
    // Column enum via the type name doesn't resolve at runtime from within
    // this QML module (ReferenceError), even with a self-import.
    readonly property int primaryColumn: root.graphVm.scoreColumn
    // SplineSeries needs at least 2 points to interpolate a curve; before any
    // performance data is loaded (or after a scenario with a single sample),
    // don't instantiate lines against the empty/underpopulated model.
    readonly property int lineCount: root.graphVm.pointCount >= 2 ? root.graphVm.totalColumnCount - 1 : 0

    GraphsView {
        id: graphsView
        anchors.fill: parent
        anchors.margins: 1
        axisX: ValueAxis {
            min: root.graphVm.axisBounds[root.graphVm.timeColumn].x
            max: root.graphVm.axisBounds[root.graphVm.timeColumn].y
        }

        Instantiator {
            model: root.lineCount

            LineFromModel {
                id: line
                required property int index
                readonly property int columnId: index + 1

                line_model: root.graphVm
                xIndex: root.graphVm.timeColumn
                yIndex: columnId
                color: root.graphVm.columnColor(columnId)
                width: 3
                visible: !!root.columnVisibility[root.graphVm.columnName(columnId).toLowerCase()]
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
