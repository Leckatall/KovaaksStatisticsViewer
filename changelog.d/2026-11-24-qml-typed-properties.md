---
type: internal
area: User Interface
---
Replaced untyped `var` properties in `src/ui/qml/` with concrete types where one exists, so qmllint and qmlls can check what is reached through them: `list<int>`/`list<real>`/`list<string>`/`list<bool>` for plain value lists (identity column widths and headers, TierBar fills/missing/labels, progress-grid row tops, visible graph columns, settings-page issue bookkeeping), `color` for table and header colours, `string` for string-list delegates, `Item`/`Popup` for item handles, and `EditableExpressionNode` for the expression editor's nodes and ancestor chain.
`BenchmarkTableModel` is now a registered, uncreatable QML type, so `BenchmarkEditorTable.tableModel` is typed and its `typeof … === "function"` guards around `identityTexts()`/`tierHeaders()` are gone. `GraphViewModelBase` changed from `QML_ANONYMOUS` to a named uncreatable type so `GraphCanvasWithTooltip.graphVm` can be typed with it.
The expression editor now casts the node with `as` when handing it to each subtype-specific field editor, which qmllint flagged once the node was typed.
View models stay `var`: the Qt Quick tests pass JS or `QtObject` fakes for every one of them, and `ScenarioHistoryPanel` passes `ControlPanel` local `QtObject` adapters as `graphVm` and `visualSettings` in production too.
Properties are never typed with the app's own QML components: the tests import component directories as `file:///` while `import KovaaksStatsViewer` loads the same files from `qrc:/`, so the same name resolves to two incompatible types.
