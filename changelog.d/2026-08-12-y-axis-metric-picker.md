---
type: added
area: Graphing
user: You can now choose which metric's y-axis is labelled on the scenario graph, from Settings → Graph Lines. The choice is remembered across launches.
---
`GraphCanvas` gains `yAxisColumn`/`labelledYAxisColumn` properties; `labelledYAxisColumn()` resolves the
requested column against `visibleColumns`, falling back to the first visible column and then to
`GraphViewModelBase::yAxisColumn()` when nothing is visible. `drawAxes()` now labels
`labelledYAxisColumn()` instead of the VM's hardcoded default. The selection is view state, not model
state: `GraphViewModel`/`PlaytimeGraphViewModel` are unchanged. Persisted as a stable column key (not
index) in a new `Settings { category: "graph" }` block in `Main.qml`, plumbed through
`DashboardGraphCanvas.qml` and `GraphCanvasWithTooltip.qml`. Picked via a new `ComboBox` on
`SettingsDialog.qml`'s Graph Lines page, listing only currently-visible columns and writing the stored
key only on explicit `onActivated`, so temporarily hiding a column doesn't clobber the preference.
