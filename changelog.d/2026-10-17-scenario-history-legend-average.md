---
type: added
area: Graphing
user: Scenario history now has clickable metric toggles and a rolling average of your last 10 scores.
---
`CompletionHistoryUseCase` computes the trailing 10-run score average over the whole scenario history before range projection, so the visible range never changes a run's value. `ScenarioHistoryPanel` reuses `ControlPanel` (new `showConfigureButton`) as an always-visible legend, and `VisualSettingsManager.isHistorySeriesVisible`/`setHistorySeriesVisible` are shared by the legend and the View > Scenario History Lines menu; a missing `"6"` entry defaults to shown.
