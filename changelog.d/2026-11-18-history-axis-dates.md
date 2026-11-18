---
type: changed
area: Graphing
user: The scenario history graph labels its runs with dates instead of run numbers; hovering shows the run's date and time and its run number.
---
`CompletionHistoryViewModel` sets a run-date delegate on its run-index x axis every `refresh()`, built from the visible rows' `(run_index, start_time_ms)`; the axis keeps its run spacing, bounds and tick positions, only the labels change.
Tick labels are the local start date (`MMM d`), hover labels add the time to the minute (`MMM d, HH:mm`); both add the year when the first and last visible runs fall in different calendar years.
A value between runs (the half-run padding around a lone visible run) takes the nearest visible run's date; with no visible rows the labels are empty.
The x axis is built with `ValueAxis::Options::distinctLabels = false`, so ticks stay one per run step even when neighbouring runs share a date and repeat it.
`GraphCanvasWithTooltip.qml` gains an optional `hoverDetail` (xRaw → one line, drawn under the header with no swatch); `ScenarioHistoryPanel.qml` uses it for `Run: #N` and titles the header `Date`.
The scenario history capture x title is now `Date (runs counted in order)`.
