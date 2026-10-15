---
type: added
area: Scenario & run selection
user: Click on the Scenario History graph to select the run nearest the click; the run list scrolls to it and the run graph shows it.
---
`ICompletionHistoryUseCase::select_run(run_index)` makes the history's run at that 1-based index current through `ISessionController::setCurrentRun`; an index outside the history does nothing. `CompletionHistoryViewModel::selectRun(int)` forwards it for indices on the graph.
`GraphCanvasWithTooltip` gains `clickable` and `pointClicked(dataX)`: with `clickable`, its hover MouseArea accepts the left button, ignores clicks outside `canvas.plotArea`, and reports the x of the nearest point (`valuesAtX(mouse.x).xRaw`, the same sample the hover tooltip shows). Hover tooltips are unchanged. `ScenarioHistoryPanel` turns it on and calls `historyVm.selectRun(Math.round(dataX))`; the selected-run marker then moves with the selection.
`RunListModel::rowOf(hash, startTimeMs)` (Q_INVOKABLE). `RunListView.syncCurrentToSelection()` sets the ListView's `currentIndex` to the selected run's row and `positionViewAtIndex(row, ListView.Contain)` whenever the selected run, the run count or the model changes, so the run list follows (and scrolls to) a run selected anywhere: the list, Recent, or the history graph. `highlightFollowsCurrentItem: false`, since the cards draw their own selection.
Depends on the selected-run marker on the Scenario History graph (the `selected_run_index` contract methods it extends).
Tests: `CompletionHistoryUseCaseTest.SelectRunMakesThatHistoryRunCurrent`, `CompletionHistoryViewModelTest.SelectRunForwardsOnlyRunsOnTheGraph`, `DashboardCleanSceneTest.ClickingTheHistoryGraphSelectsTheNearestRun` (real scene: click near run #1 of 3 selects it, marks it, moves the run list's current index; a click left of the plot changes nothing).
