import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One compact, single-line run row: [scenario] date | score | accuracy | PB.
// The column widths come from RunListView so every row (and its column header)
// lines up; the full run label and shot counts are in the hover tooltip.
ItemDelegate {
    id: root

    required property real accuracy
    readonly property color currentRunColor: Qt.tint(palette.base, Qt.rgba(palette.accent.r, palette.accent.g, palette.accent.b, 0.18))
    property string currentRunHash
    property double currentRunStartTimeMs: 0
    required property string hash
    required property int hits
    required property int index
    readonly property bool isCurrentRun: hash === currentRunHash && startTimeMs === currentRunStartTimeMs
    required property bool personalBest
    required property string runLabel
    required property string scenarioName
    required property real score
    required property int shots
    required property double startTimeMs

    // Recent mixes scenarios, so it names each run's scenario; a single scenario's
    // run list does not repeat it.
    property bool showScenarioName: false
    property real dateColumnWidth: 80
    property real scoreColumnWidth: 48
    property real accuracyColumnWidth: 48
    property real personalBestColumnWidth: 28
    property int rowHeight: 24

    readonly property string dateText: Qt.formatDateTime(new Date(startTimeMs),
                                                         showScenarioName ? "MMM d, HH:mm" : "yyyy-MM-dd  HH:mm")
    readonly property string scoreText: score.toFixed(0)
    readonly property string accuracyText: (accuracy * 100).toFixed(1) + "%"

    signal runSelected(string hash, double startTimeMs)

    implicitHeight: rowHeight
    objectName: "runItem_" + index
    topPadding: 0
    bottomPadding: 0
    leftPadding: 8
    rightPadding: 6
    width: ListView.view ? ListView.view.width : implicitWidth

    Accessible.name: (showScenarioName ? scenarioName + ", " : "") + dateText
                     + ", score " + scoreText + ", accuracy " + accuracyText
                     + (personalBest ? ", personal best" : "")

    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: runLabel + "\n" + qsTr("Score %1 · Accuracy %2 · %3/%4 hits")
                  .arg(scoreText).arg(accuracyText).arg(hits).arg(shots)
                  + (personalBest ? "\n" + qsTr("Personal best when it was played") : "")

    background: Rectangle {
        readonly property color rowColor: root.index % 2 === 0
                                          ? root.palette.base
                                          : Qt.lighter(root.palette.base, 1.25)
        readonly property color baseColor: root.isCurrentRun ? root.currentRunColor : rowColor

        color: Qt.lighter(baseColor, root.down ? 1.4 : root.hovered ? 1.2 : 1.0)
        border.width: root.isCurrentRun ? 1 : 0
        border.color: root.palette.accent

        Rectangle {
            // Accent bar marks the run the graphs are showing.
            visible: root.isCurrentRun
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: root.palette.accent
        }
    }

    contentItem: RowLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: root.showScenarioName
            elide: Text.ElideRight
            objectName: "runScenario_" + root.index
            text: root.scenarioName
        }
        Label {
            Layout.fillWidth: !root.showScenarioName
            Layout.preferredWidth: root.showScenarioName ? root.dateColumnWidth : 0
            elide: Text.ElideRight
            objectName: "runDate_" + root.index
            opacity: root.showScenarioName ? 0.75 : 1.0
            text: root.dateText
        }
        Label {
            Layout.preferredWidth: root.scoreColumnWidth
            horizontalAlignment: Text.AlignRight
            font.bold: root.personalBest
            objectName: "runScore_" + root.index
            text: root.scoreText
        }
        Label {
            Layout.preferredWidth: root.accuracyColumnWidth
            horizontalAlignment: Text.AlignRight
            objectName: "runAccuracy_" + root.index
            text: root.accuracyText
        }
        Item {
            Layout.preferredWidth: root.personalBestColumnWidth
            Layout.fillHeight: true

            Rectangle {
                anchors.centerIn: parent
                visible: root.personalBest
                width: personalBestLabel.implicitWidth + 8
                height: personalBestLabel.implicitHeight + 2
                color: root.palette.accent
                objectName: "personalBestBadge_" + root.index
                radius: 3

                Label {
                    id: personalBestLabel
                    anchors.centerIn: parent
                    color: root.palette.base
                    font.bold: true
                    font.pixelSize: 10
                    text: qsTr("PB")
                }
            }
        }
    }

    onClicked: root.runSelected(hash, startTimeMs)
}
