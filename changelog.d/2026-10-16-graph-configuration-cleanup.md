---
type: internal
area: Graphing
---
Cleanup pass over the graph configuration branch.
Graphs settings page: the series and axis Repeaters are now keyed by a string of row keys (`axisKeys` / `seriesKeys`), and each delegate looks its row up in the current draft. Before, the models were the draft's row arrays; every keystroke in a width or fixed-bound field republished `graphDrafts`, which handed the Repeater a new array, so it rebuilt every row and the field being typed in lost focus after one character. Covered by `test_typingSeveralCharactersIntoARowFieldKeepsItsFocus`.
`GraphSettingsEditor` looks up field errors in one per-graph map (`issuesByField`) instead of rescanning `draftIssues` for every label.
`GraphLineStyleControls.qml` is the one line-style combo plus marker checkbox, shared by `GraphGear` and `GraphSettingsEditor`; the connection modes, range presets, "All runs/All time" label and whole-number formatting moved to `GraphCatalog.js`. The gear reads its input limits from `settingsVm.graphLimits` rather than hard-coded copies.
`SettingsViewModel`: `quickSettings()` and `draftEntry()` share `preferencesEntry()`; visit begin/revert/end share `replaceWorkingCopy()`; issue conversion goes through `toIssue()` / `issueList()`; field edits use one parse-and-store lambda; series override edits go through `editSeriesOverride()`, which drops an empty override.
Contracts: `connectionModeName()` / `connectionModeFromName()` replace the copies in the codec and the view model; `SeriesStyle` replaces `SeriesStyleDefault` and `ResolvedSeriesStyle`, with `resolvedSeriesStyle()` shared by `GraphPresentation` and the settings page; `offersSeries()` replaces the inline lookups; `PlaytimeView::fullExtent` is a `DayInterval`; `GraphConfigurationChange` no longer carries a `graphs` list that duplicated `sections` (use `anyGraph()`).
`IGraphConfigurationUseCase::editorSeed()` is gone (a visit copies `current()`); `IGraphConfigurationService::replace()` takes its candidate by value and moves it into the snapshot.
`applyAxisPolicy()` (axis_policy) and `utcMidnightMs()` (date_time_axis) replace per-view-model copies. `GraphCanvas` no longer forces markers for Points only, because `SeriesPainter` already marks every unconnected series.
Codec: `series_codec::exactKeys` takes a span and replaces `hasExactly`; `decimalId` lost its always-false `zeroAllowed` parameter.
