pragma ComponentBehavior: Bound
import QtQuick

// The six-dot grip of a draggable row. Decoration only: the owner supplies the handlers.
Item {
    id: root

    property color dotColor: palette.light
    property string dotObjectNamePrefix: ""

    implicitWidth: 12
    implicitHeight: 24

    Repeater {
        model: 6

        Rectangle {
            id: dot

            required property int index

            objectName: root.dotObjectNamePrefix !== "" ? root.dotObjectNamePrefix + dot.index : ""
            width: 3
            height: 3
            x: 1.5 + (dot.index % 2) * 6
            y: 4.5 + Math.floor(dot.index / 2) * 6
            color: root.dotColor
            radius: width / 2
            Accessible.ignored: true
        }
    }
}
