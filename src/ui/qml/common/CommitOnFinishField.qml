import QtQuick
import QtQuick.Controls

// A field that reaches the view model only on editingFinished. SettingsDialog offers Save, and commits
// on Save or close, only while the focused field's text differs from `committedText`.
TextField {
    required property string committedText
    readonly property bool uncommittedEdit: text !== committedText
}
