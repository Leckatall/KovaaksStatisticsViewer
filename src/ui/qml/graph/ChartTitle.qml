import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// The one title style of every chart panel: the Scenarios-tab graphs and the Benchmarks-tab history
// cards. Change the style here and every chart title follows.
Label {
    horizontalAlignment: Text.AlignLeft
    elide: Text.ElideRight
    font.weight: Font.DemiBold
    font.pixelSize: 14
}
