---
type: fixed
area: Scenario & run selection
user: The scenario panel no longer resizes every time you click a scenario or a run. Its width is set from your scenario list when a profile loads, and never exceeds a third of the window.
---
`SelectionPanel` drops its ineffective `implicitWidth` for a `Layout.preferredWidth` derived from the new `ScenarioBrowserViewModel::longestScenarioName` (computed over the unfiltered catalogue, emitted only on change) measured via `TextMetrics`, clamped between the search row's chrome width and `maximumPanelWidth` (`window.width / 3`, passed from `Main.qml`). `RunListView`'s title now elides with `Layout.preferredWidth: 0` and `ScenarioSearchPanel`'s search field carries an explicit preferred width, so a model reset can no longer move the panel's size hint.
