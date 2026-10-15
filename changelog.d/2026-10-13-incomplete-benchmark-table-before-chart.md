---
type: changed
area: Benchmarks
user: An incomplete benchmark's page now shows the progress table before the playtime chart, in the same order as a tracked benchmark.
---
`incompleteComponent` in `BenchmarkWorkspace.qml`: moved the `BenchmarkProgressGrid` (`benchmarkProgressGrid`) up to sit directly after the "Edit benchmark" button, ahead of the "Total playtime" row and the `playtimeHistoryCard`. The trackable page already ordered summary, table, charts; the incomplete page had the table last, below the chart.
The "Total playtime" row stays with the chart it describes. No object names changed.
New test `test_incompletePageListsTheTableBeforeTheChart` in `tst_BenchmarkWorkspace.qml` pins the vertical order of message, issues, edit button, table, playtime summary and chart.
