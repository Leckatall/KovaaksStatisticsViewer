import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// The capture preview's footer buttons: rounded, 34 px high, filled with the highlight colour when
// `highlighted`. Lucide SVGs stroke with `currentColor`, which Fusion does not tint, so the icon colour
// is always set here.
Button {
    id: button

    readonly property bool pointerHovered: hover.hovered

    implicitHeight: Math.max(34, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 14
    icon.width: 16
    icon.height: 16
    icon.color: enabled ? palette.buttonText : Qt.alpha(palette.buttonText, 0.4)

    // A HoverHandler rather than `hovered`: the latter stays false where the platform turns hover
    // effects off.
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && ToolTip.text !== ""
    ToolTip.delay: Theme.toolTipDelay

    background: Rectangle {
        readonly property color fill: button.highlighted ? button.palette.highlight : button.palette.button

        implicitWidth: 34
        implicitHeight: 34
        radius: 4
        opacity: button.enabled ? 1 : 0.5
        color: button.down ? Qt.darker(fill, 1.2) : button.pointerHovered ? Qt.lighter(fill, 1.15) : fill
        border.width: button.visualFocus ? 2 : 1
        border.color: button.visualFocus ? button.palette.accent : Qt.lighter(button.palette.button, 1.3)
    }
}
