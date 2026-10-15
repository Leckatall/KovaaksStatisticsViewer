---
type: changed
area: Benchmarks
user: A personal best above the top tier is now marked with a small chevron past the end of the tier bar instead of a diamond.
---
`TierBar.qml`: the `<bar>_aboveTop` marker now draws a right-pointing chevron (a `Canvas`, objectName `<bar>_aboveTopChevron`) in the top tier's colour with a base-coloured halo, instead of the rotated square the blocker marker also uses. Tooltip, accessible name and the `_aboveTop` item are unchanged. `tst_BenchmarkWorkspace.qml` checks the chevron exists.
