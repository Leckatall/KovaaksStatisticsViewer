---
type: changed
area: Benchmarks
user: With "End date ranges at latest recorded run" on, the personal-best rank graph now ends at your latest run, holding your best flat through the runs since you last improved.
---
`BenchmarkTrackingUseCase::applyRankRange` used to anchor each rank graph at its own series' last point, which for the personal-best graph was the day of its last PB: an anchored range ended there, and the held level was never drawn through the runs since.
Both rank graphs now share one anchor, the UTC day of `Benchmark::memberRunStarts().back()` (nothing when it is empty). The cut-back rule, `NoRelevantHistory` and `RankRangeView::anchoredEnd` keep their meaning, and the recent graph's anchor (already its last play day) is unchanged.
