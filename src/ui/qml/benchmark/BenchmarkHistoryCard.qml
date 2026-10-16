import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    property var historyModel
    property string namePrefix: "history"
    // benchmarkRank or benchmarkPlaytime.
    required property string graphKey
    property var settingsVm: null

    signal moreGraphSettingsRequested(string graphKey)

    readonly property bool hasData: root.historyModel !== undefined
                                    && root.historyModel !== null
                                    && root.historyModel.hasData === true
    readonly property string metricName: root.historyModel ? root.historyModel.metricName : ""

    Accessible.role: Accessible.Graphic
    Accessible.name: root.metricName
    Accessible.description: qsTr("%1 history chart").arg(root.metricName)

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true

            ChartTitle {
                objectName: root.namePrefix + "LineName"
                Layout.fillWidth: true
                text: root.metricName
            }
            GraphGear {
                graphKey: root.graphKey
                settingsVm: root.settingsVm
                onMoreSettingsRequested: key => root.moreGraphSettingsRequested(key)
            }
        }

        Label {
            objectName: root.namePrefix + "EmptyLabel"
            Layout.fillWidth: true
            visible: !root.hasData
            wrapMode: Text.WordWrap
            color: root.palette.placeholderText
            text: root.historyModel ? root.historyModel.emptyStateText : ""
        }
        GraphRangeHint {
            objectName: root.namePrefix + "EmptyHint"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            visible: !root.hasData && root.historyModel !== null && root.historyModel !== undefined
                     && root.historyModel.noObservationsInRange === true
        }
        Label {
            objectName: root.namePrefix + "NoImprovementLabel"
            Layout.fillWidth: true
            visible: root.hasData && root.historyModel.noImprovementPoints === true
            wrapMode: Text.WordWrap
            color: root.palette.placeholderText
            text: qsTr("This range has no improvement points, so Points only draws nothing. "
                       + "Choose a line style to see your rank held through it.")
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            active: root.hasData
            visible: root.hasData
            sourceComponent: GraphCanvasWithTooltip {
                objectName: root.namePrefix + "Canvas"
                graphVm: root.historyModel
                visibleColumns: [BenchmarkHistoryViewModel.Value]
                showSeriesNames: true
                pointMarkers: root.historyModel.pointMarkers
                connectionMode: root.historyModel.connectionMode
                xLabel: qsTr("Date")
            }
        }
    }
}
