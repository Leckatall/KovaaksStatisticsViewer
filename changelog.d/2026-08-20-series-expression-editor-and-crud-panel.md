---
type: added
area: Graphing
user: Build and edit graph-line expressions with a visual tree editor, and manage graph lines in place — reorder, rename, recolor, edit expressions, add, or delete them. Edits are previewed as a draft and only applied when you confirm, or discarded if you cancel.
---
`SeriesExpressionEditorModel` provides a mutable, QML-facing expression tree with strict persistence-map conversion through the existing expression parser.
`SettingsViewModel::beginExpressionEdit` seeds a fresh editor from a live series configuration.
`ExpressionTreeEditor.qml` turns the gallery prototype into an application component, loading only the relevant field editor and rendering child slots through one repeater.
`ExpressionEditorDialog` creates a fresh tree editor on each open and forwards only accepted edits to `SettingsViewModel`.
`SeriesConfigDraftPanel` now exposes the full draft-backed series CRUD workflow and opens the expression editor for every series type.
Fixed along the way: ExpressionTreeEditor keeps loaded field editors and collapsed ancestor cards synchronized with expression-tree mutations and selection changes; SeriesConfigDraftPanel now preserves row delegates throughout a drag, previews the destination without mutating the row model, and derives the destination from the drag origin.
Test coverage added for expression-editor refresh behavior and series CRUD panel drag reordering.
