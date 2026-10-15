import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// The actions sit beside the name while the name can keep a comfortable width,
// and otherwise move to their own row together: never some on each row. The
// actions row has a fixed implicit width, so the switch cannot oscillate.
GridLayout {
    id: editorToolbar
    objectName: "editorToolbar"

    // Read-only here: a plain-JS test double is copied on assignment, so commands go out through
    // the signals below and the caller runs them on its own handle.
    property var manager: null
    property bool libraryCollapsed: false
    property int sessionRevision: 0
    property real availableWidth: 0

    signal libraryToggleRequested()
    signal nameEdited(string name)
    signal undoRequested()
    signal pasteRequested()
    signal saveRequested()

    readonly property alias nameField: benchmarkNameField
    readonly property real inlineNameWidth: 240
    readonly property bool actionsInline: editorToolbar.availableWidth
        >= libraryToggle.implicitWidth + nameRow.spacing + inlineNameWidth
           + columnSpacing + editorActions.implicitWidth
    columns: actionsInline ? 2 : 1
    columnSpacing: 8
    rowSpacing: 6

    // A text field that shows the model value whenever it is not being edited; typing into a
    // TextField destroys a plain `text:` binding, and these fields outlive the element they edit.
    component BoundField: TextField {
        id: boundField
        property string boundText
        selectByMouse: true
        Binding {
            target: boundField
            property: "text"
            value: boundField.boundText
            when: !boundField.activeFocus
            restoreMode: Binding.RestoreNone
        }
    }

    RowLayout {
        id: nameRow
        Layout.fillWidth: true
        spacing: 8
        ToolbarButton {
            id: libraryToggle
            objectName: "libraryToggleButton"
            icon.source: editorToolbar.libraryCollapsed ? "../icons/panel-left-open.svg"
                                                        : "../icons/panel-left-close.svg"
            display: AbstractButton.IconOnly
            Accessible.name: editorToolbar.libraryCollapsed ? qsTr("Show the library list")
                                                            : qsTr("Hide the library list")
            ToolTip.text: editorToolbar.libraryCollapsed
                ? qsTr("Show the library list")
                : qsTr("Hide the library list to give the table the full width")
            onClicked: editorToolbar.libraryToggleRequested()
        }
        BoundField {
            id: benchmarkNameField
            objectName: "benchmarkNameField"
            boundText: editorToolbar.manager ? editorToolbar.manager.benchmarkName : ""
            placeholderText: qsTr("Benchmark name")
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            onEditingFinished: editorToolbar.nameEdited(text)
        }
        // The view model's own dirty state: it compares the draft with the copy it
        // was opened from or saved to, so undoing back to that copy clears it.
        Label {
            objectName: "unsavedLabel"
            visible: editorToolbar.manager?.dirty === true
            text: qsTr("• Unsaved")
            color: Qt.alpha(editorToolbar.palette.accent, 0.8)
            Accessible.name: qsTr("The benchmark has unsaved changes")
        }
    }
    RowLayout {
        id: editorActions
        objectName: "editorActions"
        // Inline it sits at the right end of the name row; on its own row it
        // stays right-aligned under the name.
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        spacing: 4
        Label {
            objectName: "draftStatusBadge"
            Layout.rightMargin: 2
            text: editorToolbar.manager?.draftTrackable ? qsTr("Trackable") : qsTr("Incomplete")
        }
        Rectangle {
            Layout.rightMargin: 8
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: Theme.classificationColor(editorToolbar.manager?.draftTrackable
                ? "Trackable" : "Incomplete")
        }
        ToolbarButton {
            objectName: "undoButton"
            icon.source: "../icons/undo-2.svg"
            display: AbstractButton.IconOnly
            enabled: editorToolbar.sessionRevision >= 0 && editorToolbar.manager?.canUndo === true
            Accessible.name: qsTr("Undo the last table edit")
            ToolTip.text: qsTr("Undo the last table edit")
            onClicked: editorToolbar.undoRequested()
        }
        ToolbarButton {
            objectName: "pasteButton"
            icon.source: "../icons/clipboard-paste.svg"
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Paste spreadsheet cells at the current cell or rank header")
            ToolTip.text: qsTr("Paste spreadsheet cells at the current cell or rank header")
            onClicked: editorToolbar.pasteRequested()
        }
        ToolbarButton {
            objectName: "saveButton"
            icon.source: "../icons/save.svg"
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Save")
            ToolTip.text: qsTr("Save the benchmark")
            onClicked: editorToolbar.saveRequested()
        }
    }
}
