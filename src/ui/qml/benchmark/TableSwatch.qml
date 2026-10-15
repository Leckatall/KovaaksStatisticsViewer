import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// Takes no focus, so clicking it never moves the table's current cell.
AbstractButton {
    id: swatch
    required property color swatchColor
    implicitWidth: 14
    implicitHeight: 14
    focusPolicy: Qt.NoFocus
    Accessible.role: Accessible.Button
    background: Rectangle {
        color: swatch.swatchColor
        radius: 3
        border.color: swatch.hovered ? swatch.palette.highlight : swatch.palette.mid
    }
}
