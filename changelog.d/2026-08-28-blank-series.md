---
type: changed
area: Graphing
user: A series can now exist with no expression — it simply plots nothing until you give it one. "Add series" now creates a blank series instead of a default score line.
---
Series validation now allows a null top-level `SeriesConfig::expression`; a null operand *inside* an expression (a half-built node with a missing input) is still rejected as before. `validateConfig` skips expression validation when the top-level expression is null; the recursive `validate_input` path keeps reporting `MissingExpressionInput` for nested nulls.
Additive to the JSON persistence: `series_config_store` encodes a blank series' expression as JSON `null` and decodes `null` back to a null `Expression` (schemaVersion unchanged). `SettingsViewModel::createComputedSeries`/`updateComputedSeries` treat an empty expression map from QML as blank rather than rejecting it, so `SeriesConfigDraftPanel.qml`'s "Add series" button no longer hand-builds a default `{ kind: "primitive", … }` expression. Evaluation, the DSL text codec, and the C++→QML transport already tolerated a null expression.
Removed the unused singular `validateSeriesConfig` overload; its former callers (tests only) use `validateSeriesConfigs`.
