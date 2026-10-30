---
type: added
area: Benchmarks
user: New recent-average rank graph on the Benchmarks page, showing how your latest five runs per scenario rank over time, next to your personal-best rank.
---
`BenchmarkProjection::recentAverageRankHistory` — one point per play day, built in `evaluateBenchmark`'s existing chronological sweep: each entry keeps its latest five scores, normalized on its own ladder, averaged with equal weight and unplayed entries at zero. Trackable only, never stored; the last point equals the mean of the current per-scenario recent averages. `RankHistoryWindow::improvements` is renamed `points` since it now windows both rank series.

New `GraphKey::BenchmarkRecentRank` (`benchmarkRecentRank`) with the same descriptor as `BenchmarkRank` (90 days anchored to the last point, neutral `rank` series, tier axis). Additive, so no graph-configuration schema bump: documents without the key load its defaults. `BenchmarkTrackingUseCase` resolves its range into `BenchmarkWorkspaceSnapshot::recentRank` (anchored at the last play day) and announces range-only moves through `onRankChanged`, whose callback now names which rank graph moved.

`BenchmarkHistoryViewModel` gains the `RecentAverageRank` metric; `noImprovementPoints` becomes `noPointsInRange` with a metric-worded `noPointsText`. `BenchmarkTrackingViewModel` owns a third history (`recentRankHistory`) on its own date axis, and every history now defaults to the union of all three extents. `BenchmarkTrackableBody.qml` puts the two rank cards side by side when wide with playtime full width beneath, and stacks all three when narrow.
