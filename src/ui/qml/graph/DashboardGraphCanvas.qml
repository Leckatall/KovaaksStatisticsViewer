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

    // Raised by the line toggles' "Configure..." button.
    signal configureLinesRequested

    function seriesForColumn(column) {
        if (!root.graphVm) return null
        const series = root.graphVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].column === column) return series[i]
        }
        return null
    }

    function nameForColumn(column) {
        const series = root.seriesForColumn(column)
        return series ? series.name : ""
    }

    // "Score" or "Score Total, Expected Final Score": every line the Y-axis numbers measure.
    function namesForColumns(columns) {
        const names = []
        for (let i = 0; i < columns.length; ++i) {
            const name = root.nameForColumn(columns[i])
            if (name !== "") names.push(name)
        }
        return names.join(", ")
    }

    Layout.fillHeight: true
    Layout.fillWidth: true

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 12
    }

    RowLayout {
        anchors.fill: parent
        spacing: 6

        // The line legend runs down the panel's left side, beside the title as well as the plot,
        // so it costs plot width rather than height. It scrolls when the panel is too short.
        ControlPanel {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            objectName: "graphLineToggles"
            visible: root.visualSettings.controlPanelVisible !== false
            graphVm: root.graphVm
            visualSettings: root.visualSettings

            onConfigureLinesRequested: root.configureLinesRequested()
        }
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 4

            ChartTitle {
                id: scenarioTitleLabel

                Layout.fillWidth: true
                Layout.topMargin: 4
                objectName: "scenarioTitleLabel"
                // The VM's title is empty while no run is selected; the panel keeps a neutral name.
                text: root.graphVm.scenarioTitle !== "" ? root.graphVm.scenarioTitle : qsTr("Run graph")
            }
            // One always-present, always-filling slot so the plot and the empty-state messages occupy
            // the exact same area — the panel's height must not change with contentState.
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
                                text: root.namesForColumns(canvasWithTooltip.labelledYAxisColumns)
                            }
                            GraphCanvasWithTooltip {
                                id: canvasWithTooltip
                                objectName: "scenarioGraphCanvas"
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
}
