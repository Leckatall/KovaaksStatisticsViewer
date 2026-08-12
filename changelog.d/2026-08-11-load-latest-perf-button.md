---
type: fixed
area: Graphing
user: "Have Graph Load Latest Performance File" now always loads your most recent run. Previously it only appeared to work before you'd selected anything else — once you picked a different scenario or run, clicking it just reloaded that selection instead of your latest run.
---
The button previously called `GraphViewModel::fetchData("")`, which only re-pulled the series for
whatever run `ISessionController` currently had loaded, rather than asking for the latest one. This
looked correct as long as the current perf happened to already be the latest run, but once a different
run had been selected in between, "load latest" just reloaded that selection instead of the actual
newest run. It now calls a dedicated `GraphViewModel::fetchLatestData()` slot, which delegates to
`IGraphUseCase::load_latest_perf()` -> `ISessionController::setCurrentPerfToLatest()` ->
`IProfileService::getLatestPerf()`. The graph refresh itself still happens through
`ISessionController::currentPerfChanged`, the same signal that drives every other current-perf change
(file load, run selection).
