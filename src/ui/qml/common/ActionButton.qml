import QtQuick
import QtQuick.Controls

// Buttons only press on Space by default. Enter acts like Space, on release, so a held key acts once
// instead of its repeats running on into whatever takes focus next.
Button {
    id: button

    property bool _enterDown: false

    function isEnter(event: KeyEvent): bool {
        return event.key === Qt.Key_Return || event.key === Qt.Key_Enter
    }

    Keys.onPressed: event => {
        if (!button.isEnter(event)) return
        event.accepted = true
        if (!event.isAutoRepeat) button._enterDown = true
    }
    Keys.onReleased: event => {
        if (!button.isEnter(event) || event.isAutoRepeat) return
        event.accepted = true
        if (!button._enterDown) return
        button._enterDown = false
        button.clicked()
    }
    onActiveFocusChanged: if (!activeFocus) _enterDown = false
}
