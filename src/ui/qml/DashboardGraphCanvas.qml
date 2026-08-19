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
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
