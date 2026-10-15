---
type: added
area: Benchmarks
user: Each scenario's progress bar now shows your personal best right where its fill ends, in the colour of the rank you have reached.
---
`TierBar` gains `valueText`/`valueColor` and a `_value` label placed `valueGap` past `fillEnd` (the right edge of the furthest chunk with any fill).
When the label would run off the bar (a full bar, or the above-top mark's space), it moves inside the fill's end and switches to the same dark/light on-fill contrast colour the chunk labels use, since the rank colour may not read on a tier colour.
A centred per-tier threshold label that would touch the value label is hidden (`coversValue()`), so the two never overlap; the next tier's threshold is still in the grid's "Next tier" column.
Hidden for unplayed/unmatched bars, like the threshold labels.
`BenchmarkProgressGrid` feeds it `personalBestText` (empty for "—") and `attainedTierColor` (placeholder text colour when unranked); no view-model change.
Tests: new `test_personalBestSitsWhereTheFillEnds`; the threshold-centring test blanks the PB on the rows it measures.
