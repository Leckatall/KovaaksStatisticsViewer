import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

TextField {
    id: field

    property var submit: null
    // Focus goes to this item before the name is submitted: the request may open the Manager's
    // modal "already holds scenarios" prompt, which hands focus back on closing to whatever held
    // it when it opened, and the field is hidden by then. A hidden field must not stay its focus
    // scope's focus child either, or focusing that scope would hand active focus straight back.
    property Item focusReturnTarget: null
    // TextInput emits accepted() but leaves Return unaccepted, so it would propagate to an
    // enclosing view whose edit trigger then acts on its current cell.
    property bool swallowReturn: false

    signal done()

    function returnFocus() {
        if (focusReturnTarget === null) return
        field.focus = false
        focusReturnTarget.forceActiveFocus()
    }

    onAccepted: {
        if (text.trim() === "") return
        returnFocus()
        if (submit(text)) {
            text = ""
            done()
        } else if (focusReturnTarget !== null) {
            field.forceActiveFocus()
        }
    }
    Keys.onEscapePressed: {
        text = ""
        returnFocus()
        done()
    }
    Keys.priority: swallowReturn ? Keys.AfterItem : Keys.BeforeItem
    Keys.onReturnPressed: (event) => event.accepted = swallowReturn
    Keys.onEnterPressed: (event) => event.accepted = swallowReturn

    Accessible.name: placeholderText
}
