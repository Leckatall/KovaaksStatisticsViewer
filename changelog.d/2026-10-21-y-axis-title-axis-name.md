---
type: changed
area: Graphing
user: The Y-axis title now shows the name of the axis instead of listing every line on it; a line with its own scale still shows its own name.
---
`GraphCanvas::labelledYAxisTitle` replaces the QML name-joining in `DashboardGraphCanvas` and `ScenarioHistoryPanel`; VMs fill the new `SeriesModel::yAxisName`.
