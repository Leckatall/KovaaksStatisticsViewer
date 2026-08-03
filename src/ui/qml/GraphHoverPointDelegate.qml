import QtQuick

Rectangle {
    id: delegate
    width: 4; height: 4; radius: 4; color: "#4DD0E1"
    property real pointValueX
    property real pointValueY

    HoverHandler {
        id: hoverHandler
        target: Text {
            parent: delegate
            visible: hoverHandler.hovered
            color: "white"
            font.bold: true
            text: `(${delegate.pointValueX.toFixed(2)}, ${delegate.pointValueY.toFixed(2)})`
        }
    }
}
