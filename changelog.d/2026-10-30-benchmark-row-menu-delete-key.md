---
type: added
area: Benchmarks
user: In the Benchmark Manager, right-click a scenario and choose Group Scenarios (Ctrl+G) to group it, and press Delete to remove the selected scenarios.
---
`Group Scenarios` replaces the `Move to group` submenu, and `assignTargets` is removed from the view model with it. Ctrl+G moves from a `Shortcut` onto that item's `Action`, so Qt draws the hotkey in the menu. Delete goes through `BenchmarkEditorTable.handleCommandKey` and only acts when a Scenario cell is current or selected, because `selectedIds()` maps group and rank cells to their row's scenario too.
