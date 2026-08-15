---
type: fixed
area: User Interface
user: The y-axis picker in the Graph Lines settings now lists one entry per axis, so columns that share an axis (Score Total, Expected Final Score, Expected Final Score (5s)) no longer show up as separate, functionally identical choices.
---
`GraphViewModel::columnYAxis(int column)` — new invokable exposing `kColumnMeta[column].yAxis` (the
grouping introduced in b396d2f) to QML.
`SettingsDialog.qml`'s `graphLinesPage.visibleAxisColumns` dedupes `visibleColumns` by that axis
group, keeping the first visible column per axis as its representative; `yAxisColumnComboBox` now
sources its model, `currentIndex`, `displayText` and `onActivated` from that list instead of the raw
column list.
