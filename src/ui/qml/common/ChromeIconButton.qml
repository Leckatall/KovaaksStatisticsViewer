import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// An icon-only button for panel chrome. Lucide SVGs stroke with `currentColor`, which Qt renders black
// and Fusion does not tint, so the icon colour is always set here.
TooltipToolButton {
    id: button

    // Holds the highlighted look for a button whose popup is open.
    property bool active: false

    readonly property bool emphasised: pointerHovered || down || active || visualFocus

    display: AbstractButton.IconOnly
    icon.width: 16
    icon.height: 16
    icon.color: emphasised ? palette.buttonText : Qt.alpha(palette.buttonText, 0.6)

    background: Rectangle {
        radius: 4
        color: button.down || button.active ? Qt.alpha(button.palette.buttonText, 0.14)
             : button.pointerHovered ? Qt.alpha(button.palette.buttonText, 0.08)
             : "transparent"
        border.width: button.visualFocus ? 1 : 0
        border.color: button.palette.highlight
    }
}
