---
type: added
area: Benchmarks
user: Drag scenarios between rows in the benchmark editor to set their order, within a group or into another one.
---
`BenchmarkEditor::assignScenarios` takes an optional `before` entry (the next unmoved entry when it is itself moving), surfaced as the third argument of `BenchmarkManagerViewModel::assignScenarios`. `BenchmarkEditorTable` resolves a hover over a scenario or threshold cell's upper or lower half into that insertion point and draws a line across the row; drops onto group cells, the group list and the Uncategorized zone still append.
