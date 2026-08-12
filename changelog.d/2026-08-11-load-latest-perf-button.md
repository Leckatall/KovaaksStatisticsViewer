---
type: fixed
area: Graphing
user: "Have Graph Load Latest Performance File" now actually reloads the newest run instead of just re-rendering whatever was already on screen.
---
The button previously called `GraphViewModel::fetchData("")`, which only re-pulled the series for the
currently selected run and never asked the session for the latest one. It now calls a dedicated
`GraphViewModel::fetchLatestData()` slot, which delegates to `IGraphUseCase::load_latest_perf()` ->
`ISessionController::setCurrentPerfToLatest()` -> `IProfileService::getLatestPerf()`. The graph refresh
itself still happens through `ISessionController::currentPerfChanged`, the same signal that drives every
other current-perf change (file load, run selection).
