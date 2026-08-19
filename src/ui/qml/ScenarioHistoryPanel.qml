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
    readonly property int yAxisColumn: {
        for (let i = 0; i < root.plottable.length; ++i) {
            const column = root.plottable[i]
            const s = root.seriesForColumn(column)
            if (s && s.id === root.historyAxisSettings.yAxisColumnKey) return column
        }
        return -1
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

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "historyTitleLabel"
            text: root.historyVm.scenarioTitle
        }

        Loader {
            Layout.fillHeight: true
            Layout.fillWidth: true
            active: root.historyVm.runCount >= 2
            sourceComponent: Component {
                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    YAxisTitle {
                        labelObjectName: "historyYAxisTitleLabel"
                        plotArea: historyCanvas.plotArea
                        text: {
                            const s = root.seriesForColumn(historyCanvas.labelledYAxisColumn)
                            return s ? s.name : ""
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
                    }
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("At least 2 runs are needed to draw history.")
            verticalAlignment: Text.AlignVCenter
            visible: root.historyVm.runCount < 2
        }
    }
}
