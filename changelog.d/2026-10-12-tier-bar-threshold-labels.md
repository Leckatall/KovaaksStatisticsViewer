---
type: changed
area: Benchmarks
user: Each scenario's progress bar now shows the score needed to reach every tier in the middle of that tier's segment.
---
`BenchmarkProgressGridModel` rows gain `thresholdTexts`: one `formatBenchmarkNumber` string per tier in ladder order (the score that reaches that tier), empty when the entry has no usable threshold for it.
`TierBar.qml` gains `labels` (one short text per tier) and `labelAt()`: each chunk draws its text centred, at a pixel size derived from the bar's height (62%, at least 8). Two labels per chunk keep the number readable across the fill's edge: a dim one in the text colour over the empty track, and a copy clipped to the fill's width in dark or light text chosen from the tier colour's luminance. A chunk's label is hidden when its chunk is hatched (`missing`), and on an `unplayed` or `unmatched` bar, where the status text owns the middle.
`BenchmarkProgressGrid.qml` passes each row's `thresholdTexts`; the summary's average-rank bar passes none, so it is unchanged. The recent-average marker and the tier tick are drawn above the labels and may cover a digit when the value sits in the middle of a chunk.
Tests: `BenchmarkProgressGridModel.RowsCarryTheScoreThatReachesEachTier` (ui-cpp), `test_eachChunkShowsTheScoreThatReachesItsTierCentred` (tst_BenchmarkWorkspace.qml).
