import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var graphVm
    required property var visualSettings
    readonly property var visibleColumns: {
        const result = [];
        for (const id of root.graphVm.enabledSeriesIds) {
            if (!root.visualSettings.isSeriesVisible(id)) continue;
            const column = root.graphVm.columnForSeriesId(id);
            if (column !== -1) result.push(column);
        }
        return result;
    }

    readonly property int yAxisColumn: {
        const cols = root.visibleColumns;
        for (const column of cols) {
            if (root.graphVm.seriesIdForColumn(column) === root.visualSettings.graphAxisSeriesId) {
                return column;
            }
        }
        return cols.length > 0 ? cols[0] : -1;
    }

    function nameForColumn(column) {
        if (!root.graphVm) return ""
        const series = root.graphVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].column === column) return series[i].name
        }
        return ""
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
            id: scenarioTitleLabel

            Layout.fillWidth: true
            Layout.topMargin: 4
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "scenarioTitleLabel"
            text: root.graphVm.scenarioTitle
        }
        // One always-present, always-filling slot so the plot and the empty-state messages occupy the
        // exact same area — the panel's height must not change with contentState.
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
            objectName: "graphContentArea"

            Loader {
                anchors.fill: parent
                active: root.graphVm.contentState === GraphViewModel.HasData
                sourceComponent: Component {
                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        YAxisTitle {
                            labelObjectName: "scenarioYAxisTitleLabel"
                            plotArea: canvasWithTooltip.plotArea
                            text: root.nameForColumn(canvasWithTooltip.labelledYAxisColumn)
                        }
                        GraphCanvasWithTooltip {
                            id: canvasWithTooltip
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            graphVm: root.graphVm
                            visibleColumns: root.visibleColumns
                            yAxisColumn: root.yAxisColumn
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                objectName: "graphNoPerformanceLabel"
                text: qsTr("Selected run does not support performance analysis.")
                visible: root.graphVm.contentState === GraphViewModel.NoPerformanceData
                wrapMode: Text.WordWrap
            }

            Label {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                objectName: "graphNoRunSelectedLabel"
                text: qsTr("No run selected.")
                visible: root.graphVm.contentState === GraphViewModel.NoRunSelected
            }
        }
    }
}
