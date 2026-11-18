---
type: internal
area: Architecture
---
UI-layer `ValueTransform` renamed `ValueDelegate` (`value_delegate.h`): `formatter` → `axisFormatter`, `format()` → `axisLabel()`, `SeriesModel::transform` → `delegate`, `ValueAxis::setTransform` → `setDelegate`. Scale/offset remain a display-only conversion; raw points stay raw.
`ValueDelegate` gains an optional `hoverFormatter` and `hoverLabel()`, which falls back to `axisLabel()` when no hover formatter is set; `Axis::formatHover()` sits beside `formatTick()`.
Hover readouts go through the hover label: `GraphCanvas::valuesAtX` and `nearestPoint` format x with `formatHover`, and `SeriesModel::formattedValueAtX` uses the series' `hoverLabel`. `nearestPoint`'s y now comes from the series' own delegate instead of its Y axis' tick formatter, so both hover paths format a value the same way. Tick and end labels stay on `formatTick`.
`ValueAxis::Options` gains `distinctLabels` (default `true`, last member because `graph_vm.cpp` builds `Options` positionally); `false` skips the step coarsening that otherwise keeps adjacent tick labels distinct.
The app-layer `AxisTransformKind` and the persisted `transformKind` JSON key are deliberately unchanged.
