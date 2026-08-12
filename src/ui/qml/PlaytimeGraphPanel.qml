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
        GraphCanvasWithTooltip {
            graphVm: root.playtimeVm
            // Single series ("Playtime"), always visible.
            visibleColumns: [PlaytimeGraphViewModel.Playtime]
            showSeriesNames: false
        }
    }
}
