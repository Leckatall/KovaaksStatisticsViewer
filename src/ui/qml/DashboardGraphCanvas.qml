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

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            id: scenarioTitleLabel

            Layout.fillWidth: true
            Layout.topMargin: 4
            elide: Text.ElideRight
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "scenarioTitleLabel"
            // The VM's title is empty while no run is selected; the panel keeps a neutral name.
            text: root.graphVm.scenarioTitle !== "" ? root.graphVm.scenarioTitle : qsTr("Run graph")
        }
        // The line toggles sit inside the panel whose lines they switch.
        ControlPanel {
            Layout.fillWidth: true
            objectName: "graphLineToggles"
            visible: root.visualSettings.controlPanelVisible !== false
            graphVm: root.graphVm
            visualSettings: root.visualSettings

            onConfigureLinesRequested: root.configureLinesRequested()
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
                    ColumnLayout {
                        id: plotColumn

                        anchors.fill: parent
                        spacing: 2

                        readonly property string axisNames: root.namesForColumns(canvasWithTooltip.labelledYAxisColumns)
                        readonly property var axisSeries: root.seriesForColumn(canvasWithTooltip.labelledYAxisColumn)

                        // Lines are scaled independently, so the Y-axis numbers belong only to the
                        // labelled line(s); say so instead of implying a shared scale.
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 4
                            objectName: "yAxisScaleCaption"
                            spacing: 5
                            visible: canvasWithTooltip.hasIndependentlyScaledSeries

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                color: plotColumn.axisSeries ? plotColumn.axisSeries.color : "transparent"
                                height: 9
                                radius: 2
                                width: 9
                            }
                            Label {
                                objectName: "yAxisScaleCaptionAxis"
                                color: canvasWithTooltip.labelledYAxisColor
                                font.bold: true
                                font.pixelSize: 11
                                text: qsTr("Y axis: %1").arg(plotColumn.axisNames)
                            }
                            Label {
                                Layout.fillWidth: true
                                objectName: "yAxisScaleCaptionOthers"
                                color: root.palette.placeholderText
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                text: qsTr("· other lines are each scaled to fit; hover for their values")
                            }
                        }
                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 0

                            YAxisTitle {
                                labelObjectName: "scenarioYAxisTitleLabel"
                                plotArea: canvasWithTooltip.plotArea
                                text: plotColumn.axisNames
                                textColor: canvasWithTooltip.hasIndependentlyScaledSeries
                                           ? canvasWithTooltip.labelledYAxisColor
                                           : Qt.alpha(root.palette.text, 0.6)
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
