import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

ScrollView {
    id: control

    contentWidth: availableWidth
    clip: true
    ScrollBar.vertical: ListScrollBar {
        // A ScrollView leaves a replacement bar's geometry to the caller.
        parent: control
        x: control.width - width
        y: control.topPadding
        height: control.availableHeight
        overflowing: control.contentHeight > control.height + 1
    }
}
