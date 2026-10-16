import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var historyVm
    required property var columnVisibility
    required property var historyAxisSettings
    readonly property var plottable: [
        CompletionHistoryViewModel.Score,
        CompletionHistoryViewModel.Accuracy,
        CompletionHistoryViewModel.Shots,
        CompletionHistoryViewModel.Hits,
        CompletionHistoryViewModel.Misses
    ]
    function seriesForColumn(column) {
        const series = root.historyVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].column === column) return series[i]
        }
        return null
    }
    readonly property var visibleColumns: root.plottable.filter(column => {
        const s = root.seriesForColumn(column)
        return s && !!root.columnVisibility[s.id]
    })

    // The selected run's x position on the history graph, or -1 for none or one the range hides.
    readonly property int selectedRunIndex: root.historyVm.selectedRunIndex !== undefined
                                            && root.historyVm.selectedRunVisible === true
                                            ? root.historyVm.selectedRunIndex : -1

    readonly property int yAxisColumn: {
        const cols = root.visibleColumns;
        for (const column of cols) {
            if (root.seriesForColumn(column).id === root.historyAxisSettings.yAxisColumnKey) {
                return column;
            }
        }
        return cols.length > 0 ? cols[0] : -1;
    }

    Layout.fillHeight: true
    Layout.fillWidth: true

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 12
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        ChartTitle {
            Layout.fillWidth: true
            Layout.topMargin: 4
            objectName: "historyTitleLabel"
            text: root.historyVm.scenarioTitle
        }

        Loader {
            Layout.fillHeight: true
            Layout.fillWidth: true
            active: root.historyVm.visibleRunCount >= 1
            sourceComponent: Component {
                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    YAxisTitle {
                        labelObjectName: "historyYAxisTitleLabel"
                        plotArea: historyCanvas.plotArea
                        text: {
                            const names = []
                            for (const column of historyCanvas.labelledYAxisColumns) {
                                const s = root.seriesForColumn(column)
                                if (s) names.push(s.name)
                            }
                            return names.join(", ")
                        }
                    }
                    GraphCanvasWithTooltip {
                        id: historyCanvas

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        graphVm: root.historyVm
                        visibleColumns: root.visibleColumns
                        yAxisColumn: root.yAxisColumn
                        xLabel: qsTr("Run")
                        objectName: "historyGraphCanvas"
                        pointMarkers: root.historyVm.pointMarkers
                        connectionMode: root.historyVm.connectionMode
                        // The run selected in the run list / run graph.
                        markedX: root.selectedRunIndex > 0 ? root.selectedRunIndex : NaN
                        // A click selects the run nearest it.
                        clickable: true
                        onPointClicked: dataX => root.historyVm.selectRun(Math.round(dataX))
                    }
                }
            }
        }

        Label {
            objectName: "historyEmptyLabel"
            Layout.alignment: Qt.AlignCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.historyVm.runCount === 0
                  ? qsTr("No runs recorded for this scenario.")
                  : qsTr("No runs in the selected range.")
            verticalAlignment: Text.AlignVCenter
            visible: root.historyVm.visibleRunCount < 1
        }
    }
}
