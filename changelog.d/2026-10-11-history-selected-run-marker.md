---
type: added
area: Graphing
user: The Scenario History graph marks the selected run with a vertical line and a ring on each line's point, following your selection in the run list and Recent.
---
`ICompletionHistoryUseCase` gains `selected_run_index()` (the current run's 1-based `CompletionRow::run_index`, found by `ScenarioRunId` in the profile's completion history; nullopt with no run or a run missing from the history) and `onSelectedRunChanged()` (every `ISessionController::currentRunChanged`, unlike `onCurrentScenarioChanged`, which is gated on the scenario hash). Implemented in `CompletionHistoryUseCase`.
`CompletionHistoryViewModel::selectedRunIndex` (-1 for none) with `selectedRunIndexChanged`; `refreshSelection()` re-reads it, and `refresh()` calls it after rebuilding the series. `App::App()` wires `onSelectedRunChanged` to `refreshSelection()`.
`GraphCanvas` gains `markedX` (data units, NaN = none) and `markerColor` (default `#E040FB`, a magenta none of the history series use). `drawMarkedX()` paints after the series: a 2px rule across the plot at `markedX`, and a ring in the marker colour filled with the series colour around each visible line's point at exactly that x.
`GraphCanvasWithTooltip.markedX` passes through; `ScenarioHistoryPanel` binds it to `historyVm.selectedRunIndex`.
Tests: `CompletionHistoryUseCaseTest.SelectedRunIndex*`/`NoSelectedRunHasNoSelectedRunIndex`/`SelectedRunCallbackFiresOnEveryRunChange`, `CompletionHistoryViewModelTest.SelectedRunIndexFollowsTheUseCase`, `tst_ScenarioHistoryPanel::test_selectedRunIsMarkedOnTheCanvas`.
