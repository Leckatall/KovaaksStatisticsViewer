---
type: changed
area: Benchmarks
user: A tier's score on the benchmark bars no longer disappears when your PB sits on top of it; it moves to the middle or start of its segment instead.
---
`TierBar.qml`: a threshold label the value (PB) label would cover is no longer hidden outright. `labelPlacementAt(index, labelWidth)` tries the chunk's end, then its middle, then its start, and returns `"end" | "middle" | "start"`, or `""` (hidden) only when the PB label touches the label in all three. Each chunk delegate exposes it as `labelPlacement`, and both the dim label and its fill-coloured clipped copy take their `horizontalAlignment` from it via `labelAlignment()`.
The PB label's position (`valueLeft`, `valueInside`) is unchanged: it never moves for a threshold label.
`coversValue(index, labelWidth)` became `coversValue(index, labelWidth, placement)`, built on the new `labelSpanLeft()`/`labelSpanWidth()` (the label clamped to the chunk's inset width, as its elided text is).
Among the moved placements (middle, start), one also clear of the recent-average marker (`coversMarker()`, the tick ±4 px) is preferred; if neither is, the first one clear of the PB is used even under the marker. The marker alone never moves a label off its chunk's end, so the existing VT FlyTS case (marker over the last character of Iron's 3,500) is unchanged.
In the `rich` dataset this brings back Air CELESTIAL's Iron 1,200 and 1wall6targets' Platinum 5,600 (demo fixture), and Air CELESTIAL's Bronze 1,050 (many-tiers fixture). At 960 wide, 1wall6targets' Platinum 5,600 has no spot clear of both the inside PB and the marker, so it sits at the chunk start under the marker.
Tests: `test_thresholdLabelMovesOutOfThePersonalBestsWay` drives a standalone `TierBar` through every placement, the hidden case and the marker preference; `test_personalBestSitsWhereTheFillEnds` now expects the covered label to move rather than hide.
