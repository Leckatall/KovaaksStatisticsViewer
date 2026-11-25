---
type: changed
area: Benchmarks
user: The line on the summary's average-rank bar now marks your recent average rank, as it does on the scenario rows, instead of your completed rank.
---
`Benchmark::recentAverageRank()` — the last point of `recentAverageRankHistory()`, so the summary and the recent-rank graph share one derivation; none without a member run (unlike `averageRank()`, which is 0 with nothing played).
`BenchmarkTrackingViewModel`: `completedRankPosition` replaced by `recentAverageRankPosition` (-1 when absent) and `recentAverageRank` (the `describeAverageRank` text); `BenchmarkStatusSummary.qml` draws it with TierBar's recent-average `marker`, at a fractional position rather than snapped to a tier boundary.
TierBar's `tick` (the summary was its only user) removed; `markerHovered` added so the bar's own tooltip steps aside for the marker's.
Table capture: `TableCaptureSummary::recentAverageRankPosition` drawn with the grid rows' marker tick; `scaleX` now takes the chunk geometry so the summary bar's own chunks can use it; `kCompletedTick*` removed in favour of `kMarkerOverhang`.
