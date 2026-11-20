---
type: changed
area: Benchmarks
user: When the same scenario is matched to multiple benchmark rows, the benchmark playtime graph now counts each run once for each of those rows. With "End date ranges at latest recorded run" enabled, recent-day ranges on that graph end on the last day you played a scenario that contributes to the benchmark.
---
`BenchmarkTrackingUseCase::evaluate` requests run facts for the definition's mapped hashes and builds one `domain::Benchmark`.
`Benchmark::rollingPlaytime` and `Benchmark::latestRunDay` use runs from Resolved entries, once per entry.
The use case caches the full series for the current window, fills inactive days, and slices the series to the selected range.
`BenchmarkWorkspaceSnapshot` publishes `benchmark` and `visiblePlaytime`.
