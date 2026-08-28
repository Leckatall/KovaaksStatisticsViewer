---
type: internal
area: Architecture
---
Replaced the UI expression QVariantMap codec with canonical DSL text at the C++/QML seam. `SeriesExpressionEditorModel` now constructs `application::Expression` directly; the removed DSL-vs-JSON differential test covered the retired duplicate codec, while DSL, editor, view-model, and QML transport coverage remains.
