---
type: fixed
area: Benchmarks
user: With "End date ranges at latest recorded run" on, the rank history graph now ends at your latest rank improvement for every range (recent days, custom dates and all history) instead of stretching the line past it. The rank line is also drawn over the tier lines.
---
`BenchmarkTrackingUseCase::applyRankRange` anchored recent-day ranges on `Evaluation::latestRunDay`, the newest run on any of the benchmark's scenarios, improving or not, and custom dates and all history ignored the anchor flag entirely. `averageRankHistory` only has points on improvement days, so the X axis usually ended days after the last point and `BenchmarkTrackingViewModel::updateRankHistory`'s trailing carry drew the held level out to it.
The rank range now anchors on the last `averageRankHistory` day; the playtime range still anchors on `latestRunDay`.
With the anchor flag on, a calendar interval that runs past the last improvement is cut back to it (one wholly after it stays as entered), and `RankRangeView::anchoredEnd` carries that day so `BenchmarkTrackingViewModel::applyRankAxis` caps the shared all-history extent at it too.
`GraphCanvas::paint` draws reference lines before series, so the tier lines sit under the rank line.
