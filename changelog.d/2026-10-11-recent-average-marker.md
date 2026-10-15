---
type: added
area: Benchmarks
user: Each scenario's tier bar now marks your recent average with a thin tick, so you can see at a glance whether your recent runs sit below or near your personal best. Hover it for the exact value.
---
`tierScalePosition(tiers, thresholds, score)` (`benchmark_progress_grid_model.{h,cpp}`) places a score on the tier bar's chunk scale (chunk units, 0 to tier count) via `domain::normalizedTierValue`, and returns nothing without a score or when `domain::thresholdLadder` rejects the ladder, so a marker is never drawn at a made-up place.
Each progress-grid row gains `recentPosition` (-1 for no marker) and `recentDescription` ("Recent average 5,226 · Silver", the tier being the one the recent value itself attains, or Unranked).
`TierBar.qml` gains `marker`/`markerDescription`: a 4px light tick outlined in the base colour that overhangs the bar by 3px at both ends (readable over a fill and over the empty track), positioned across the chunk gaps by `scaleX()`, with a 9px-wide hover target and its own tooltip; the bar's own tooltip is suppressed while the marker is hovered. `BenchmarkProgressGrid.qml` passes each row's values.
Tests: `TierScalePosition.*` and `BenchmarkProgressGridModel.RecentAverageBelowThePersonalBestSitsLeftOfTheFillEnd` (ui-cpp), `test_recentAverageMarkerSitsOnTheChunkScale` (tst_BenchmarkWorkspace.qml).
