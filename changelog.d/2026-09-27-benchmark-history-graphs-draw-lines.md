---
type: fixed
area: Benchmarks
user: The benchmark rank and playtime history graphs now draw their lines instead of showing empty axes.
---
`BenchmarkHistoryCard.qml` never set `visibleColumns` on `GraphCanvasWithTooltip`, so `BenchmarkHistoryViewModel::series([])` returned nothing and no line was drawn. Both axes still painted: the x axis comes from the VM, and the y axis resolves through `GraphCanvas::labelledYAxisColumn()`'s fallback to `yAxisColumn()`, which bypasses the column filter. The card now passes `[BenchmarkHistoryViewModel.Value]`.
`BenchmarkHistoryUiTest.BothHistoryCanvasesPaintTheirSeries` pins it over the real `Main.qml` scene: two days of improving runs against an auto-resolved benchmark, then each history `GraphCanvas` is painted into a `QImage` and scanned for its series colour inside the plot area.
A benchmark with a single day of history still draws no line, since `GraphCanvas::drawSeries` skips series with fewer than two points.
