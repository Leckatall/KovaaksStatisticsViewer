---
type: changed
area: Benchmarks
user: The benchmark playtime graph now counts only time on scenarios actually matched to your plays, once per benchmark row they fill, and "Anchor to latest run" ends at your latest play of a matched scenario.
---
`BenchmarkTrackingUseCase::evaluate` requests run facts for the definition's mapped hashes and builds one `domain::Benchmark`.
`Benchmark::rollingPlaytime` and `Benchmark::latestRunDay` use runs from Resolved entries, once per entry.
The use case caches the full series for the current window, fills inactive days, and slices the series to the selected range.
`BenchmarkWorkspaceSnapshot` publishes `benchmark` and `visiblePlaytime`. The existing selected definition and projections mirror that evaluation until Cleanup.
