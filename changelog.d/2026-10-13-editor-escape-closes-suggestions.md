---
type: fixed
area: Benchmarks
user: In the Benchmark Manager, pressing Escape while the scenario suggestion list is open now only closes the list. A second Escape cancels the edit and keeps the old name, instead of renaming the scenario to what you had typed.
---
Bug mechanism: the scenario cell editor in `BenchmarkEditorTable.qml` handled Escape with an open suggestion list in `Keys.onPressed` (close the popup, accept). But `TableView` installs an event filter on its editor that closes the editor on the Escape key press before the item's `Keys` handlers run, so with the list open the key press closed the editor instead. `Keys.onShortcutOverride` deliberately skips marking the text cancelled while a popup is open, so `Component.onDestruction` then committed the typed text as a rename and the draft became Unsaved.
Fix: the editor's existing application-context Escape `Shortcut` (`editorEscapeShortcut`, previously enabled only while the editor's context menu was open) is now also enabled while `suggestionPopup` is open. The shortcut override is left unaccepted in that state, so the shortcut matches and consumes the Escape before any key press reaches `TableView`; it closes the context menu if open, otherwise the suggestion list. The now-unreachable Escape branch in `Keys.onPressed` is removed. With the list closed the override marks the editor cancelled as before, so a second Escape discards the edit. Return, click-to-choose and click-away commits are untouched.
The other inline editors in the table (group rename, add-category, add-subcategory, add-scenario fields) have no popups of their own, so they are unaffected.
Test: `tst_BenchmarkEditorTable.qml` `test_escapeClosesSuggestionListBeforeDiscarding` (fails before the fix: the first Escape destroyed the editor).
