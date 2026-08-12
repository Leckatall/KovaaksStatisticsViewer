---
type: changed
area: Graphing
user: The graph's y-axis title now sits snug against the tick numbers instead of leaving a fixed gap, and adjusts automatically whether the numbers are short or long.
---
`GraphCanvas::plotRect()` ([graph_canvas.cpp](src/ui/components/graph_canvas.cpp)) replaced the fixed
`kLeftMargin`/`kBottomMargin` constants with margins measured from the actual tick label text via a new
`AxisPainter::measureLabelExtent()` ([axis_painter.h](src/ui/components/axis_painter.h)), which builds a
`QFontMetricsF` from the same `Style` `paint()` already uses so measurement can't drift from what's
drawn. Left margin tracks the widest formatted y-axis tick (`QFontMetricsF::horizontalAdvance`); bottom
margin tracks the tick font's line height. Since the margin now depends on data, not just widget size,
`plotAreaChanged` is now also emitted from `setGraphVm`'s `dataUpdated`/`boundsChanged` handlers and from
`setVisibleColumns`/`setYAxisColumn`, so `YAxisTitle.qml`'s rotation math and the hover crosshair stay in
sync. `drawAxes()` and the new `plotRect()` share the y-axis lookup via a new `labelledYAxis()` helper
instead of duplicating the `series({labelledYAxisColumn()})` call.
