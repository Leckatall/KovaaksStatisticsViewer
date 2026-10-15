---
type: fixed
area: Benchmarks
user: The Benchmark Manager editor's Undo, Paste, Save and Discard are now compact icon buttons with tooltips, and they no longer split across two rows.
---
Old bug mechanism: the editor toolbar's actions were a `Flow` inside a two-column `GridLayout` that did not let it fill its cell. A wrapped `Flow` reports its wrapped width as its implicit width, so once "Discard" wrapped (as it did at the default 1180x760 after opening the demo benchmark) the layout never offered it more room, and it stayed wrapped even at 1400px wide.
`BenchmarkManagerDialog.qml`: the actions are now a `RowLayout` (`editorActions`) of icon-only `ToolbarButton`s (Lucide `undo-2`, `clipboard-paste`, `save`, `x`, the last three new under `src/ui/qml/icons/`), each with a tooltip and the previous accessible name. The `GridLayout` (`editorToolbar`) switches between two columns and one as a whole: `actionsInline` is true while the editor pane fits the library toggle, a 240px name field and the actions row's fixed implicit width, so the actions are either all beside the name or all on their own row, with no feedback loop. Object names (`undoButton`, `pasteButton`, `saveButton`, `discardButton`, `draftStatusBadge`) are unchanged.
Tests: `tst_BenchmarkManagerDialog.qml::test_editorActionsWrapAllOrNothing` resizes the dialog through narrow, default, narrow and wide widths and checks that every action shares one row, that the row matches `actionsInline`, and that the actions return beside the name at 1180px and above.
