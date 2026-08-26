---
type: removed
area: Graphing
---
`GraphViewModelBase`'s five legacy read-model methods (`plottableColumns()`, `axisBounds()`, `axisTicks()`, `seriesPoints()`, `xColumn()`) — unused by `GraphCanvas` or any QML binding, all callers already migrated to `series()`/`xAxis()`. `GraphViewModel::m_data` and `PlaytimeGraphViewModel::m_points` (storage that existed only to back `seriesPoints()`) removed alongside them.
