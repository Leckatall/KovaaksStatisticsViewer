import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var playtimeVm

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
            Layout.fillWidth: true
            Layout.topMargin: 4
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "playtimeTitleLabel"
            text: "Daily Playtime (3-day rolling average)"
        }
        // The plot and the empty state share one slot so the panel keeps its size either way.
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
            objectName: "playtimeContentArea"

            Loader {
                anchors.fill: parent
                active: root.playtimeVm.hasData
                sourceComponent: Component {
                    GraphCanvasWithTooltip {
                        objectName: "playtimeGraphCanvas"
                        graphVm: root.playtimeVm
                        // Single series ("Playtime"), always visible.
                        visibleColumns: [PlaytimeGraphViewModel.Playtime]
                        showSeriesNames: false
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                objectName: "playtimeEmptyLabel"
                text: qsTr("No playtime recorded yet.")
                visible: !root.playtimeVm.hasData
                wrapMode: Text.WordWrap
            }
        }
    }
}
