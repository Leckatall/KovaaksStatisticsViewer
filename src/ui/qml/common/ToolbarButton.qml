import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// A toolbar button sized to its content (the style's 80px minimum would not fit the row). An
// icon-only button shows its tooltip on hover.
Button {
    id: toolbarButton
    leftPadding: 8
    rightPadding: 8
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    HoverHandler { id: toolbarHover }
    ToolTip.visible: toolbarHover.hovered && ToolTip.text !== ""
    ToolTip.delay: Theme.toolTipDelay
}
