---
type: added
area: Graphing
user: The runtime graph now explains itself when there's nothing to plot — "Selected run does not support performance analysis" for CSV-only runs (or ones whose .perf is gone), and "No run selected" when none is showing — instead of a blank panel.
---
`GraphViewModel` gains a `ContentState` Q_ENUM (`NoRunSelected`/`NoPerformanceData`/`HasData`) computed in
`fetchData()` from two new `IGraphUseCase` predicates, `hasCurrentRun()` and `currentRunHasPerformance()`
(the latter keyed off loaded `Run::performance`, not the source ref). `DashboardGraphCanvas.qml` gates the
plot behind a `Loader` active only on `HasData`, with two centered fallback labels for the other states —
mirroring `ScenarioHistoryPanel`'s empty-state shape. `GraphViewModel` is now registered
uncreatable in `declare_metatypes()` so QML can switch on the enum.
