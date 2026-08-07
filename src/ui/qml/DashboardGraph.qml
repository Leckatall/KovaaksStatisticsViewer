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

    readonly property int primaryColumn: root.graphVm.scoreColumn

    readonly property int lineCount: root.graphVm.pointCount >= 2 ? root.graphVm.totalColumnCount - 1 : 0

    GraphsView {
        id: graphsView
        anchors.fill: parent
        anchors.margins: 1
        axisX: ValueAxis {
            min: root.graphVm.axisBounds[root.graphVm.timeColumn].x
            max: root.graphVm.axisBounds[root.graphVm.timeColumn].y
        }

        // Shared, fully inert axis for hidden columns. QGraphsView reserves a
        // fixed-width layout slot for every *distinct* axisY object attached
        // to any series - regardless of that axis's own visible property
        // (Qt 6.11 QGraphsView::calculateAxisCounts doesn't check isVisible()).
        // Pointing every hidden line at this one shared instance means Qt's
        // per-axis dedup only ever reserves a single slot for all of them
        // combined, instead of one slot per hidden column. AlignRight keeps
        // that one slot from shifting the plot area left.
        axisY: ValueAxis {
            visible: false
            labelsVisible: false
            gridVisible: false
            lineVisible: false
            alignment: Qt.AlignRight
        }

        Instantiator {
            model: root.lineCount

            LineFromModel {
                id: line
                required property int index
                readonly property int columnId: index + 1
                readonly property bool columnVisible: !!root.columnVisibility[root.graphVm.columnName(columnId).toLowerCase()]

                line_model: root.graphVm
                xIndex: root.graphVm.timeColumn
                yIndex: columnId
                color: root.graphVm.columnColor(columnId)
                width: 3
                visible: line.columnVisible
                pointDelegate: GraphHoverPointDelegate {}

                // Only columns that are actually rendered (the primary axis,
                // or a currently-toggled-on secondary line) get their own
                // real axis with correct bounds; everything else shares
                // graphsView's inert dummy axisY so it costs no extra layout
                // space. See comment on graphsView.axisY above.
                axisY: (line.columnId === root.primaryColumn || line.columnVisible) ? ownAxisY : graphsView.axisY

                ValueAxis {
                    id: ownAxisY
                    min: root.graphVm.axisBounds[line.columnId].x
                    max: root.graphVm.axisBounds[line.columnId].y
                    visible: line.columnId === root.primaryColumn
                    labelsVisible: line.columnId === root.primaryColumn
                    gridVisible: line.columnId === root.primaryColumn
                    lineVisible: line.columnId === root.primaryColumn
                    alignment: line.columnId === root.primaryColumn ? Qt.AlignLeft : Qt.AlignRight
                    subTickCount: 4
                }
            }

            onObjectAdded: (index, object) => graphsView.addSeries(object)
            onObjectRemoved: (index, object) => graphsView.removeSeries(object)
        }
    }
}
