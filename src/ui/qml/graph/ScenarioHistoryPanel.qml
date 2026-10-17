import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var historyVm
    required property var columnVisibility
    required property var historyAxisSettings
    property var settingsVm: null

    signal moreGraphSettingsRequested(string graphKey)
    signal seriesVisibilityRequested(string seriesId, bool shown)
    readonly property var plottable: [
        CompletionHistoryViewModel.Score,
        CompletionHistoryViewModel.Accuracy,
        CompletionHistoryViewModel.Shots,
        CompletionHistoryViewModel.Hits,
        CompletionHistoryViewModel.Misses,
        CompletionHistoryViewModel.ScoreAverage
    ]

    // Mirrors VisualSettingsManager.isHistorySeriesVisible: maps saved before the average existed lack "6".
    function isHistorySeriesVisible(id) {
        const values = root.columnVisibility
        return Object.prototype.hasOwnProperty.call(values, id) ? !!values[id] : id === "6"
    }
    function seriesForColumn(column) {
        const series = root.historyVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].column === column) return series[i]
        }
        return null
    }
    readonly property var visibleColumns: root.plottable.filter(column => {
        const s = root.seriesForColumn(column)
        return s && root.isHistorySeriesVisible(s.id)
    })

    // History exists but the range shows none of it.
    readonly property bool rangeIsEmpty: root.historyVm.runCount > 0 && root.historyVm.visibleRunCount < 1

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

    ControlPanel {
        id: legend

        objectName: "historyLineToggles"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        showConfigureButton: false
        graphVm: legendModel
        visualSettings: legendVisibility
    }

    QtObject {
        id: legendModel

        readonly property var allSeries: root.historyVm.allSeries
        readonly property var enabledSeriesIds: root.historyVm.allSeries.map(series => series.id)
    }

    QtObject {
        id: legendVisibility

        function isSeriesVisible(id) { return root.isHistorySeriesVisible(id) }
        function setSeriesVisible(id, shown) { root.seriesVisibilityRequested(id, shown) }
    }

    ColumnLayout {
        anchors.left: legend.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.rightMargin: 4

            ChartTitle {
                Layout.fillWidth: true
                objectName: "historyTitleLabel"
                text: root.historyVm.scenarioTitle
            }
            GraphGear {
                graphKey: "scenarioHistory"
                settingsVm: root.settingsVm
                onMoreSettingsRequested: key => root.moreGraphSettingsRequested(key)
            }
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

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 4
            visible: root.historyVm.visibleRunCount < 1

            Item { Layout.fillHeight: true }
            Label {
                objectName: "historyEmptyLabel"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.rangeIsEmpty ? qsTr("No runs in the selected range.")
                                        : root.historyVm.scenarioTitle === "" ? qsTr("No scenario selected.")
                                                                              : qsTr("No runs recorded for this scenario.")
            }
            GraphRangeHint {
                objectName: "historyEmptyHint"
                Layout.fillWidth: true
                visible: root.rangeIsEmpty
            }
            Item { Layout.fillHeight: true }
        }
    }
}
