---
type: changed
area: Benchmarks
user: Edit benchmarks in a compact scenario and rank table, paste spreadsheet cells, and repair invalid thresholds in place before saving.
---
`BenchmarkManagerViewModel` owns the table session: hierarchy projection, stable selection anchors, retained invalid threshold text, positional paste with expansion, one-step batch removal, and chronological Undo. Save submits a normalized candidate and adopts it only after success; a library or profile publication re-resolves the open draft without discarding edits.
`BenchmarkManagerDialog.qml` and `BenchmarkEditorTable.qml` replace the scenario cards with synchronized table views, editable rank headers and group cells, contextual controls, mapping state, and an empty-group list. Table commands commit active edits first; model resets restore the current cell and multi-row selection; rejected additions preserve their input. Shortcut and context-menu Paste/Undo operate on the session, while Escape discards an active cell edit.
`BenchmarkTableModel`, `BenchmarkCellInput`, `BenchmarkClipboardMatrix`, and `BenchmarkClipboardAdapter` provide projection, locale-aware numeric input, clipboard parsing, and clipboard access. The obsolete tree projection and its QML registrations are removed. Native, QML and real-composition tests cover these paths.
