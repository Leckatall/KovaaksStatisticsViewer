import QtQuick
import QtQuick.Controls

// Points a person whose selected range is empty at the graph's gear, where the range is widened.
Label {
    horizontalAlignment: Text.AlignHCenter
    wrapMode: Text.WordWrap
    color: palette.placeholderText
    text: qsTr("Use the graph settings button to widen the range.")
}
