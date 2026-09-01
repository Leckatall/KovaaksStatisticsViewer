---
type: changed
area: Graphing
user: Graphs now show a colored marker on each line at the hovered position instead of drawing dots for every data point.
---
`SeriesPainter::Style` retains control-point marker rendering behind `showMarkers`, which defaults to disabled.
`GraphCanvas::valuesAtX()` now supplies each sampled series' pixel y-coordinate so `GraphCanvasWithTooltip` can animate a line-colored hover marker above the existing snapped vertical indicator.
