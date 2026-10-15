import QtQuick
import QtQuick.Controls

// A vertical scroll bar for the selection lists that is always visible while the
// list has more content than fits (the style's own handle is near-invisible on the
// dark palette), so a list that ends on its last whole row still reads as scrollable.
ScrollBar {
    id: control

    // Set by the owning view: whether its content is taller than the view.
    property bool overflowing: false

    policy: overflowing ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    minimumSize: 0.08
    padding: 2
    implicitWidth: 10

    background: Rectangle {
        color: Qt.alpha(control.palette.text, 0.06)
        radius: width / 2
    }

    contentItem: Rectangle {
        implicitWidth: 6
        radius: width / 2
        color: Qt.alpha(control.palette.text, control.pressed ? 0.6 : control.hovered ? 0.5 : 0.35)
    }
}
