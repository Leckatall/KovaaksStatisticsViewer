---
type: changed
area: Benchmarks
user: In the benchmark progress grid, a tier score that sits inside a partly filled tier bar (because it moved out of the PB's way) is now outlined, so it reads as the score still to reach rather than one already reached.
---
`TierBar.qml`: new `labelInPartialFillAt(index, labelWidth, placement)` — true when the chunk's fill is partial (fill width > 0 and < chunk width) and the label's drawn span (`labelSpanLeft`/`labelSpanWidth` at its placement) starts left of the fill's end, i.e. overlaps the filled part at all. Each chunk delegate exposes `labelIsTarget` (`labelShown && labelInPartialFillAt(...)`) plus `labelSpanX`/`labelSpanW` (the label span in chunk coordinates).
A target label keeps its text, colour, placement and tooltip; it gains a 1 px, radius-2 transparent outline box (`<bar>_target_<index>`) 2 px wider than the text span on each side and 1 px in from the bar's top and bottom. Like the label, the box is drawn twice: dim (`palette.text` at 0.62) over the empty track, and in the chunk's `onFillColor` inside the fill's clip, so it reads across the fill edge.
Placement (`labelPlacementAt`) is untouched: the target decision is taken from the placement it already chose, and the outline sits within the 3 px label inset and the 4 px PB gap, so it cannot newly collide with the PB label.
In the rich driver dataset this outlines Air CELESTIAL's Iron 1,200 (driver-demo) and Bronze 1,050 (driver-many-tiers), and also Smoothbot's Platinum 2,000 and VT FlyTS's Bronze 4,000 in driver-demo and Smoothbot's Astra 2,000 in driver-many-tiers, which meet the same condition.
Pinned by `test_thresholdLabelInsideAPartialFillIsATarget` in `tests/ui/qml/tst_BenchmarkWorkspace.qml`.
