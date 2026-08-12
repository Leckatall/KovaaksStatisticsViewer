---
type: added
area: Scenario & run selection
user: Scenario list can now be sorted by run count, name, or last played, via a new sort control next to the search field.
---
`ScenarioBrowserViewModel` renamed `SortField` to `RunSortField` and added `ScenarioSortField`
(`RUN_COUNT`/`LAST_PLAYED`/`NAME`) plus `setScenarioSort()`, backed by `applyScenarioSort()` and
`refreshScenarioModel()` (called from `refresh()`).
Added `getLastRunTime()` to `IProfileService`/`ProfileService`/`UserProfile`, `ScenarioRunId::startSecond()`,
and `ScenarioSummary::last_played` (populated in `SessionController::getScenarioSummaries()`, exposed as
`ScenarioListModel::LastPlayedRole`).
`ScenarioSearchPanel.qml` gained a sort combo + direction button mirroring `RunListView.qml`'s, forwarded
through `SelectionPanel.qml`'s new `scenarioSortRequested` signal to `ScenarioBrowserViewModel::setScenarioSort()`.

