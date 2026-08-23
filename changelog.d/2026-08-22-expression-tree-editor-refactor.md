---
type: fixed
area: Settings
user: Fixed a crash when editing certain deeply nested computed-series expressions. Unfortunately this required removing the nested cards layout. Now they are listed.
---
`SeriesExpressionEditorModel` now owns a tree of typed `Editable*ExpressionNode` QObjects (`EditablePrimitiveNode`, `EditableConstantNode`, `EditableBinaryOpNode`, `EditableUnaryOpNode`, `EditableRollingMeanNode`, `EditableAverageAcrossRunsNode`) with per-property `NOTIFY` instead of a disposable `QVariantMap` snapshot tree rebuilt on every edit.
`ExpressionTreeEditor.qml`'s breadcrumb is now a flat `Repeater` over `PathCard.qml` instances instead of a `Loader` that recursively instantiated itself; field editors live in `FieldEditors.qml` and are selected via a type-keyed `Loader` instead of `Qt.createComponent().createObject()` with manual property rebinding.
This incidentally fixed a `RangeError: Maximum call stack size exceeded` that occurred when editing deeply nested expressions (e.g. `Subtract(ProjectRateToFinal(RollingMean(...)), ProjectRateToFinal(RollingMean(...)))`) — most likely caused by the old self-referencing breadcrumb `Loader`.
