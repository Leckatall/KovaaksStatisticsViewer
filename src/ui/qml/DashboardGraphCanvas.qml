import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var columnVisibility
    required property var graphVm
    required property var graphAxisSettings
    readonly property var visibleColumns: {
        const cols = root.graphVm.plottableColumns;
        const result = [];
        for (let i = 0; i < cols.length; i++) {
            if (root.columnVisibility[root.graphVm.columnKey(cols[i])]) {
                result.push(cols[i]);
            }
        }
        return result;
    }
    readonly property int yAxisColumn: {
        const cols = root.graphVm.plottableColumns;
        for (let i = 0; i < cols.length; i++) {
            if (root.graphVm.columnKey(cols[i]) === root.graphAxisSettings.yAxisColumnKey) {
                return cols[i];
            }
        }
        return -1;
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
