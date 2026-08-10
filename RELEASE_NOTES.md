# Release Notes

What's new for people using the app. For the technical record, see [CHANGELOG.md](CHANGELOG.md).

## v0.4.0-alpha

### Scenario & run selection
- New scenario search panel — search and browse scenarios instead of navigating them manually.
- Per-scenario run list, with sorting options for how runs are ordered.
- "Recent runs" quick-access list showing your latest runs across all scenarios at a glance.
- Various fixes and polish to the selection interface based on early use.

### Graphing
- New playtime graph, tracking play time alongside your performance stats.
- Reworked graph rendering for smoother lines and finer visual control, including improved curve interpolation.
- Graph axes now use "nice" tick positioning for more readable scales.
- Hover tooltips on graphs for inspecting individual data points.

### Under the hood
- Profile cache storage switched to a single file rather than a directory, simplifying data management.
