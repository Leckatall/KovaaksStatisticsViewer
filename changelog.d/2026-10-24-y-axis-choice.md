---
type: changed
area: Graphing
user: Settings > Graph Lines now has a "Y-axis" picker that lists your axes (plus any line that has no axis of its own) instead of every line, and the run graph labels the first visible line on the axis you pick. The picker's list also highlights the entry under the pointer.
---
The run graph's labelled Y axis is now `GraphConfiguration::runAxisLabel`, an `std::optional<RunAxisLabel>` where `RunAxisLabel = std::variant<AxisId, SeriesId>`; a line with no `yAxisId` keeps its own scale in `GraphViewModel::series()`, so it is its own axis.
New `runAxisLabelFor`, `reconciledRunAxisLabel` and `runAxisLabelLines` in `graph_configuration.h`: a line on a named axis stands for that axis, and a label whose axis or line is gone is cleared.
`SettingsViewModel` reconciles the label after every Graph Lines edit and on `setRunAxisLabel`, and exposes `runAxisLabelChoices` (`{key, name}`, keys `axis:<id>` / `series:<id>`) and `runAxisLabelKey` in place of `runAxisLabelSeriesId`.
`IGraphUseCase::getAxisLabelSeriesIds()` and `GraphViewModel::axisLabelSeriesIds` replace the single series id; `DashboardGraphCanvas.qml` labels the first visible column whose series is in that list, falling back to the first visible column as before.
Graph configuration document schema 6 stores `runAxisLabel` as `null`, `{"axis": "<id>"}` or `{"series": "<id>"}`. Versions 4 and 5 `runAxisLabelSeriesId` and the legacy `graphAxis/seriesId` QSettings value are read as a line and moved to that line's axis on load, so an old choice of any line on a shared axis becomes that axis.
The dialog's stale `visibleAxisColumns` (de-duplicating by `displayPosition`, which filtered nothing) and its `seriesName` helper are gone; the combo's delegate now binds `highlighted` to `highlightedIndex`.
