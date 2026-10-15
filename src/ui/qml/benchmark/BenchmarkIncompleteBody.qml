import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

OverflowScrollView {
    id: incompleteScroll

    property var trackingVm
    property real bodyWidth
    property bool wide

    signal manageBenchmarksRequested()

    ColumnLayout {
        width: incompleteScroll.bodyWidth
        spacing: 8

        Label {
            objectName: "incompleteBody"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("This benchmark is not ready to rank. Finish its setup to unlock ranks.")
        }

        Repeater {
            model: (incompleteScroll.trackingVm && incompleteScroll.trackingVm.completenessIssues)
                   ? incompleteScroll.trackingVm.completenessIssues : []
            delegate: Label {
                required property var modelData
                required property int index
                objectName: "completenessIssue_" + index
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "• " + modelData
            }
        }

        Button {
            objectName: "editBenchmarkButton"
            text: qsTr("Edit benchmark")
            onClicked: incompleteScroll.manageBenchmarksRequested()
        }

        // Same order as the trackable page: the table, then the charts.
        BenchmarkProgressGrid {
            objectName: "benchmarkProgressGrid"
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            grid: incompleteScroll.trackingVm ? incompleteScroll.trackingVm.progressGrid : null
            benchmarkKey: incompleteScroll.trackingVm ? incompleteScroll.trackingVm.selectedName : ""
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: qsTr("Total playtime")
                color: incompleteScroll.palette.placeholderText
            }
            Label {
                objectName: "incompleteTotalPlaytime"
                text: incompleteScroll.trackingVm ? incompleteScroll.trackingVm.totalPlaytime : ""
                font.bold: true
            }
        }

        BenchmarkHistoryCard {
            objectName: "playtimeHistoryCard"
            Layout.fillWidth: true
            Layout.minimumHeight: 200
            namePrefix: "playtimeHistory"
            historyModel: incompleteScroll.trackingVm ? incompleteScroll.trackingVm.playtimeHistory : null
        }
    }
}
