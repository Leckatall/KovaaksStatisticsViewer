---
type: changed
area: Graphing
---
`GraphViewModel` now pulls series points and the time-axis duration through discrete
`IGraphUseCase` accessors (`getSeriesConfigs`/`getSeriesValues`/`getAxes`/`getRunDuration`)
instead of the bundled `get_resolved_graph()`; metadata refreshes no longer evaluate series
values, and points arrive pre-assembled rather than zipped in the view model.
