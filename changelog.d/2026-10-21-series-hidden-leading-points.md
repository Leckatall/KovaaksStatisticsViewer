---
type: added
area: Graphing
user: Run Graph lines can now hide their first few points (0 to 10), so a projection like Expected Final Score no longer stretches the graph with its unreliable start. The setting lives in a new options menu on each Graph Lines row, which also holds the delete button.
---
`SeriesPresentation::hiddenLeadingPoints` (max `kMaximumHiddenLeadingPoints`, validated as `HiddenLeadingPointsOutOfRange`), applied in `GraphUseCase::getSeriesValues` after the average evaluator runs. Persisted by bumping the graph configuration schema to 5 (series section version 4); older documents load with 0. Expected Final Score and Expected Final Score (5s) default to hiding 1 point. `SettingsViewModel::setSeriesHiddenLeadingPoints` and `maximumHiddenLeadingPoints` back the options popup, hosted once in `SeriesConfigDraftPanel.qml` (rows rebuild on every edit) and opened from each row's "⋯" button in `SeriesConfigEditorDelegate.qml`.
