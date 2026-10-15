import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

TextField {
    id: editor

    property Item cell: null
    property Item table: null

    property bool committed: false
    property bool cancelled: false
    // Captured once: the delegate underneath can be recycled while the editor lives.
    property int kind: -1
    property string anchorEntry
    property string anchorTier
    property string anchorGroup
    property string original
    property TableView ownerView: null
    property int suggestionIndex: 0
    readonly property var suggestionPopupView: suggestionPopup

    // A spanning category's name is edited across the whole span.
    width: cell.face.width
    horizontalAlignment: cell.columnKind === table.thresholdKind ? TextInput.AlignRight : TextInput.AlignLeft
    Accessible.name: qsTr("Edit %1").arg(cell.Accessible.name)

    function commitNow() {
        if (editor.committed || editor.cancelled) return
        editor.committed = true
        table.commitCell(editor.kind, editor.anchorEntry, editor.anchorTier, editor.anchorGroup,
                        editor.text, editor.original)
    }

    function suggestions() {
        const query = editor.text.trim().toLowerCase()
        if (editor.kind !== 2 || query === "" || !table.manager) return []
        const catalogue = table.manager.scenarioCatalogue || []
        return catalogue.filter(item => item.name.toLowerCase().indexOf(query) !== -1)
    }

    function chooseSuggestion(item) {
        if (!item) return
        const entryId = editor.anchorEntry
        editor.cancelled = true
        suggestionPopup.close()
        table.activeEditor = null
        Qt.callLater(table.scenarioIdentityRequested, entryId, item.name, item.hash)
        editor.ownerView.closeEditor()
    }

    Component.onCompleted: {
        editor.kind = cell.columnKind
        editor.anchorEntry = cell.entryId
        editor.anchorTier = cell.tierId
        editor.anchorGroup = cell.groupId
        editor.original = cell.columnKind === table.thresholdKind ? cell.editText
            : cell.columnKind === 2 ? cell.displayText : cell.groupLabelText
        editor.ownerView = cell.TableView.view
        editor.text = editor.original
        editor.selectAll()
        table.activeEditor = editor
    }
    // TableView closes an editor without commit() when the user taps elsewhere; typed
    // input must still reach the session, so only Escape discards it.
    Component.onDestruction: {
        if (table.activeEditor === editor) table.activeEditor = null
        if (!editor.committed && !editor.cancelled)
            Qt.callLater(table.commitCell, editor.kind, editor.anchorEntry, editor.anchorTier,
                         editor.anchorGroup, editor.text, editor.original)
    }
    // Paste and Undo are session commands even mid-edit: they commit the typed text as its
    // own step first, instead of the text field consuming them locally.
    Keys.onPressed: (event) => {
        if (suggestionPopup.opened && event.key === Qt.Key_Down) {
            editor.suggestionIndex = Math.min(editor.suggestionIndex + 1, editor.suggestions().length - 1)
            event.accepted = true
        } else if (suggestionPopup.opened && event.key === Qt.Key_Up) {
            editor.suggestionIndex = Math.max(editor.suggestionIndex - 1, 0)
            event.accepted = true
        } else if (suggestionPopup.opened && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            editor.chooseSuggestion(editor.suggestions()[editor.suggestionIndex])
            event.accepted = true
        } else {
            table.handleCommandKey(event)
        }
    }
    onTextEdited: {
        editor.suggestionIndex = 0
        if (editor.suggestions().length > 0) suggestionPopup.open()
        else suggestionPopup.close()
    }
    // TableView's editor event filter closes the editor on the Escape key press before
    // Keys sees it; the shortcut override sent ahead of that press is where the text is
    // marked discarded. While the menu or the suggestion list is open the override is left
    // unaccepted, so the Escape shortcut below closes it instead and no key press reaches
    // TableView.
    Keys.onShortcutOverride: (event) => {
        if (suggestionPopup.opened && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            event.accepted = true
            editor.chooseSuggestion(editor.suggestions()[editor.suggestionIndex])
            return
        }
        if (event.key !== Qt.Key_Escape || editorMenu.opened || suggestionPopup.opened) return
        editor.cancelled = true
        event.accepted = true
    }
    TableView.onCommit: editor.commitNow()

    // Neither the menu nor the suggestion list holds focus (see below), so they cannot close
    // themselves on Escape. A window shortcut outside an open popup is not matched, hence
    // the application context; it is enabled only while one of them is open. The menu is
    // the more recent popup, so it closes first.
    Shortcut {
        objectName: "editorEscapeShortcut"
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: editorMenu.opened || suggestionPopup.opened
        onActivated: {
            if (editorMenu.opened) editorMenu.close()
            else suggestionPopup.close()
        }
    }

    // Replaces the style's menu, whose Paste and Undo act on the text field alone. Session
    // commands are deferred so the triggering menu finishes before they close the editor
    // and destroy this menu with it. Neither the menu nor its items take focus: TableView
    // closes and commits the editor as soon as focus leaves it.
    ContextMenu.menu: Menu {
        id: editorMenu
        popupType: Popup.Item
        modal: false
        focus: false
        MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Cut"); enabled: editor.selectedText !== ""; onTriggered: editor.cut() }
        MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Copy"); enabled: editor.selectedText !== ""; onTriggered: editor.copy() }
        MenuItem { focusPolicy: Qt.NoFocus; objectName: "sessionPasteAction"; text: qsTr("Paste"); onTriggered: Qt.callLater(table.pasteFromClipboard) }
        MenuItem { focusPolicy: Qt.NoFocus; objectName: "sessionUndoAction"; text: qsTr("Undo"); onTriggered: Qt.callLater(table.undo) }
        MenuSeparator {}
        MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Select All"); onTriggered: editor.selectAll() }
    }

    Popup {
        id: suggestionPopup
        parent: editor
        x: 0
        y: editor.height
        width: Math.max(editor.width, 240)
        height: Math.min(suggestionList.contentHeight + 8, 180)
        popupType: Popup.Item
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        padding: 4
        contentItem: ListView {
            id: suggestionList
            clip: true
            model: editor.suggestions()
            delegate: ItemDelegate {
                id: suggestion
                required property var modelData
                required property int index
                objectName: "scenarioSuggestion_" + suggestion.modelData.hash
                width: suggestionList.width
                text: qsTr("%1 · %2").arg(suggestion.modelData.name).arg(suggestion.modelData.hash)
                highlighted: index === editor.suggestionIndex
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Use played scenario %1, hash %2")
                    .arg(suggestion.modelData.name).arg(suggestion.modelData.hash)
                onClicked: editor.chooseSuggestion(suggestion.modelData)
            }
        }
    }
}
