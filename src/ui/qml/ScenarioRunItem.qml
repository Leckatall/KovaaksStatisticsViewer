import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The app's clickable run card: a rounded two-line card (bold "<scenario> (<date>)" title,
// then score, accuracy and a PB badge). It lifts on hover, shows a pointing-hand cursor, and
// marks the run the graphs are showing with a light accent tint and an accent border.
ItemDelegate {
    id: root

    required property real accuracy
    property string currentRunHash
    property double currentRunStartTimeMs: 0
    required property string hash
    required property int index
    readonly property bool isCurrentRun: hash === currentRunHash && startTimeMs === currentRunStartTimeMs
    required property bool personalBest
    required property string runLabel
    required property real score
    required property double startTimeMs

    // A light accent tint for the run the graphs are showing; the accent border carries the selection.
    readonly property color currentRunColor: Qt.tint(palette.base, Qt.alpha(palette.accent, 0.08))
    // Hover as seen by a HoverHandler: Control.hovered stays false where the platform disables
    // hover effects, which would leave the card without its hover lift.
    readonly property bool pointerOver: cardHover.hovered
    readonly property string scoreText: root.score.toFixed(0)
    readonly property string accuracyText: (root.accuracy * 100).toFixed(1) + "%"

    signal runSelected(string hash, double startTimeMs)

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    objectName: "runItem_" + index
    padding: 10
    width: ListView.view ? ListView.view.width : implicitWidth

    Accessible.name: root.runLabel + ", score " + root.scoreText + ", accuracy " + root.accuracyText
                     + (root.personalBest ? ", personal best" : "")

    HoverHandler {
        id: cardHover
        cursorShape: Qt.PointingHandCursor
    }

    background: Rectangle {
        readonly property color restColor: root.isCurrentRun ? root.currentRunColor : root.palette.base

        // Hover and press tint towards the text colour: Qt.lighter() barely moves a near-black base.
        color: Qt.tint(restColor, Qt.alpha(root.palette.text, root.down ? 0.12 : root.pointerOver ? 0.07 : 0.0))
        border.color: root.isCurrentRun ? root.palette.accent
                                        : root.pointerOver ? Qt.lighter(root.palette.mid, 1.6) : root.palette.mid
        radius: 8
    }
    contentItem: ColumnLayout {
        spacing: 2

        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.bold: true
            objectName: "runLabel_" + root.index
            text: root.runLabel
        }
        RowLayout {
            Layout.fillWidth: true

            Label {
                objectName: "runScore_" + root.index
                text: qsTr("Score %1").arg(root.scoreText)
            }
            Label {
                objectName: "runAccuracy_" + root.index
                text: qsTr("Acc %1").arg(root.accuracyText)
            }
            Item {
                Layout.fillWidth: true
            }
            Rectangle {
                visible: root.personalBest
                Layout.preferredHeight: personalBestLabel.implicitHeight + 4
                Layout.preferredWidth: personalBestLabel.implicitWidth + 10
                color: root.palette.accent
                objectName: "personalBestBadge_" + root.index
                radius: 4

                Label {
                    id: personalBestLabel
                    anchors.centerIn: parent
                    color: root.palette.base
                    font.bold: true
                    text: qsTr("PB")
                }
            }
        }
    }

    onClicked: root.runSelected(hash, startTimeMs)
}
