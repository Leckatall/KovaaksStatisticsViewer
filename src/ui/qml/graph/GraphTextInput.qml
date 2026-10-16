import QtQuick
import QtQuick.Controls

// Shows `shown`, the view model's text for one field, unless the person is typing in it, and reports every
// edit exactly as typed; what the text means (and whether it parses) is the view model's decision.
TextField {
    id: input

    property string shown
    // The graph `shown` belongs to. A graph switch always replaces the text, typing in progress or not.
    property string graphKey
    property string _shownFor

    signal edited(string text)

    onShownChanged: {
        if (!activeFocus || _shownFor !== graphKey) text = shown
        _shownFor = graphKey
    }
    onActiveFocusChanged: if (!activeFocus) text = shown
    onTextEdited: edited(text)
    Component.onCompleted: {
        text = shown
        _shownFor = graphKey
    }
}
