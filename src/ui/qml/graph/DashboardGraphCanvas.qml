import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var graphVm
    required property var visualSettings
    property var settingsVm: null
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
        const labelled = root.graphVm.axisLabelSeriesIds;
        for (const column of cols) {
            if (labelled.includes(root.graphVm.seriesIdForColumn(column))) {
                return column;
            }
        }
        return cols.length > 0 ? cols[0] : -1;
    }

    // Raised by the line toggles' "Configure..." button.
    signal configureLinesRequested
    signal moreGraphSettingsRequested(string graphKey)

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

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.rightMargin: 4

                ChartTitle {
                    id: scenarioTitleLabel

                    Layout.fillWidth: true
                    objectName: "scenarioTitleLabel"
                    // The VM's title is empty while no run is selected; the panel keeps a neutral name.
                    text: root.graphVm.scenarioTitle !== "" ? root.graphVm.scenarioTitle : qsTr("Run graph")
                }
                GraphGear {
                    graphKey: "run"
                    settingsVm: root.settingsVm
                    onMoreSettingsRequested: key => root.moreGraphSettingsRequested(key)
                }
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
                                text: canvasWithTooltip.labelledYAxisTitle
                            }
                            GraphCanvasWithTooltip {
                                id: canvasWithTooltip
                                objectName: "scenarioGraphCanvas"
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                graphVm: root.graphVm
                                visibleColumns: root.visibleColumns
                                yAxisColumn: root.yAxisColumn
                                pointMarkers: root.graphVm.pointMarkers
                                connectionMode: root.graphVm.connectionMode
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
