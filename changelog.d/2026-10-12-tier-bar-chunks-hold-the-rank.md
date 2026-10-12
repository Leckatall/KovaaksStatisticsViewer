---
type: changed
area: Benchmarks
user: Each section of a benchmark progress bar is now the range in which you hold that tier, so the bar ends in the colour of the rank you have reached, and each section is labelled at its start with the score that reaches it. The top tier's section fills completely once you reach it; below the first tier the bar stays empty.
---
Tier bar chunk k used to be the progress *toward* tier k (from t[k-1], or 0, up to t[k]), so a partly filled chunk was drawn in the colour of a tier not yet reached. Chunk k is now the range *in* tier k: from t[k] up to t[k+1], with the last chunk full once t[top] is reached and no fill below t[0].
`scenarioChunkFills` (`benchmark_progress_grid_model.{h,cpp}`) implements the new mapping. A chunk the score has entered gets at least `kEnteredChunkFill` (1e-6), so a score exactly on a threshold still has a fill, and `TierBar.fillWidthAt` draws any non-zero fill at least `minimumFillWidth` (3 px) wide so the colour is visible. The old zero-first-threshold special case is gone, because any score >= 0 enters a chunk whose t[0] is 0.
`missingThresholdChunks` now hatches a chunk when its own threshold or the *next* tier's is unusable (the top chunk needs only its own), not when the previous tier's is.
`rankChunkFills` (summary average-rank bar): rank r fills tier r's chunk (index r - 1) with its fractional part, and the top chunk is full from r >= tier count. `BenchmarkTrackingViewModel::completedRankPosition` is now the completed tier's 0-based chunk index (the tick sits at that chunk's start), and `TierBar.tick` is "the boundary where chunk `tick` starts", with 0 = bar start now allowed.
`tierScalePosition` (recent-average marker) maps the domain's normalized value p to `p >= n ? n : max(0, p - 1)`, so a recent average below the first tier sits at the bar's start. The row's `recentDescription` tier now comes from `domain::normalizedTierValue` directly rather than from the bar position.
`progressDescription` no longer reads the fills: it walks the thresholds to find the first tier not reached. "Highest tier attained" and the `aboveTop` edge state now test only the last chunk's fill.
`TierBar` threshold labels are left-aligned with a `labelInset` of 3 px, and `coversValue` checks the left-aligned span against the PB value label.
Tests: the C++ fill, missing-chunk, marker-scale and rank-fill expectations are rewritten for the new chunk meaning, and the QML fixtures (`TestDoubles.js`) and `tst_BenchmarkWorkspace` (tick position, left-aligned labels, PB/marker chunk indices) are updated to match.
