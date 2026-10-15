---
type: changed
area: Benchmarks
user: The average rank in the Benchmarks tab now reads as a tier and progress ("Bronze · 60% to Silver") instead of a bare number like 2.6, both in the rank graph's hover tooltip and in the summary's average-rank bar tooltip.
---
New `describeAverageRank(rank, tierNames)` in `presentation/benchmark_format.{h,cpp}` turns the evaluator's mean normalized rank (0 Unranked, k = k-th tier, fraction = progress toward the next) into "`<tier>` · `x`% to `<next tier>`", "Unranked · x% to `<first tier>`" below the first tier, and "`<top tier>` · top tier reached" at the top (the wording the progress grid's scenario descriptions use). The percentage is floored and capped at 99 so it never reads 100% before the next tier, and a value within 1e-9 of a whole number snaps onto it so a mean that lands a rounding error short of a tier boundary is that tier at 0%, not the previous tier at 99%. With no tiers it falls back to `formatBenchmarkNumber`.
`BenchmarkHistoryViewModel` gives the rank series' own `ValueTransform` a formatter built from the tier names, so the shared graph tooltip (`SeriesModel::formattedValueAtX`) shows "Personal-best average rank: Bronze · 57% to Silver"; the Y axis keeps its separate tier-name tick formatter.
`BenchmarkTrackingViewModel::averageRank` now carries that text ("Unavailable" when not trackable) instead of the locale-formatted number; its only consumer is `BenchmarkStatusSummary.qml`, whose tooltip, `Accessible.name` and `Accessible.description` are now "Average rank: `<text>`". The number is still not shown next to the bar.
`GraphCanvasWithTooltip.qml` clamps the tooltip box inside the graph: the longer rank text can be wider than either side of the cursor, which cut it off at the graph's left edge (visible on a 12-tier benchmark).
Tests: tracking VM (mid-tier, below the first tier, exact boundary, near-boundary rounding, 99% cap, top tier), history VM tooltip values, and the QML summary tooltip/accessible text.
