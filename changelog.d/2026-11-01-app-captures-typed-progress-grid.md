---
type: internal
area: Benchmarks
---
`ProgressGridContent` — `BenchmarkProgressGridModel` builds typed rows, tiers, groups and headings, and its QML lists are derived from that one form with the same keys and values, so an exporter reads fields rather than string keys.
Display strings QML used to build now come from the presentation layer: the row's `nextText`, the grid's `headers`, and `BenchmarkTrackingViewModel`'s `nextTierBlockerText` and `summaryCaptions`. Tooltips and accessible-only strings stay in QML because captures exclude them.
