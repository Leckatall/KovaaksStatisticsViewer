---
type: added
area: Graphing
user: The scenario graph's y-axis now shows a small rotated title naming the metric its tick numbers belong to, so it stays clear even after switching the axis metric in Settings.
---
New [YAxisTitle.qml](src/ui/qml/YAxisTitle.qml): a fixed-width column with a `Label` rotated -90°, sized
and positioned against a caller-supplied `plotArea` rect so its footprint stays centered on the plot
regardless of rotation (rotation about the default center origin never moves that center point).
`GraphCanvasWithTooltip.qml` promotes `plotArea`/`labelledYAxisColumn` (already on the inner
`GraphCanvas`) to root-level readonly bindings so callers can read them without new C++. Wired into
`DashboardGraphCanvas.qml` inside a new `RowLayout`; `GraphCanvasWithTooltip`'s own internal
anchoring/hover math is untouched since it still fills its (now narrower) cell exactly as before.
`PlaytimeGraphPanel.qml` is intentionally left alone — its single hardcoded column is unambiguous.
`GraphCanvas::kLeftMargin` ([graph_canvas.cpp](src/ui/components/graph_canvas.cpp)) shrunk 55 → 40:
the old value left most of the tick-number band blank at the default `AxisPainter::Style` point size,
which read as a gap between the title and the axis it labels.
