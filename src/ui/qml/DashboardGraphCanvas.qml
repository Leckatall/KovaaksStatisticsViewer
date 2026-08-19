import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    property var seriesVisibility
    property var columnVisibility
    required property var graphVm
    required property var graphAxisSettings
    readonly property var visibleColumns: {
        const cols = root.graphVm.allColumns || root.graphVm.enabledColumns;
        const result = [];
        for (let i = 0; i < cols.length; i++) {
            const id = root.graphVm.seriesIdForColumn ? root.graphVm.seriesIdForColumn(cols[i])
                                                       : root.graphVm.columnKey(cols[i]);
            const enabled = root.graphVm.enabledSeriesIds !== undefined
                    ? root.graphVm.enabledSeriesIds.indexOf(id) >= 0
                    : root.graphVm.enabledColumns.indexOf(cols[i]) >= 0;
            if (id && enabled && SeriesVisibility.read(root.effectiveVisibility, id)) {
                result.push(cols[i]);
            }
        }
        return result;
    }

    readonly property int yAxisColumn: {
        const cols = root.visibleColumns;
        const preferred = root.graphAxisSettings.seriesId || root.graphAxisSettings.yAxisColumnKey;
        for (let i = 0; i < cols.length; i++) {
            if ((root.graphVm.seriesIdForColumn ? root.graphVm.seriesIdForColumn(cols[i])
                                                 : root.graphVm.columnKey(cols[i])) === preferred) {
                return cols[i];
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
                text: root.graphVm.columnName(canvasWithTooltip.labelledYAxisColumn)
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
