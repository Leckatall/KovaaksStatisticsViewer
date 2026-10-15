import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

ToolButton {
    id: button

    readonly property bool pointerHovered: hover.hovered

    // A HoverHandler rather than `hovered`: the latter stays false where the platform turns
    // hover effects off.
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && ToolTip.text !== ""
    ToolTip.delay: Theme.toolTipDelay
}
