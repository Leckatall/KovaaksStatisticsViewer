---
type: changed
area: Benchmarks
user: The benchmark selector shows just the name of a normal benchmark. Only benchmarks that need attention carry a label, now a coloured badge: incomplete, invalid or unsupported.
---
`BenchmarkWorkspace.qml`: the selector entries no longer append "  —  <classification>" to the delegate text. A trackable entry shows only its name. A new `selectorEntryLabel_<name>` chip (a filled pill, amber for incomplete, red for invalid and unsupported, dark text) sits at the right of the row and is visible for every classification except trackable (compared case-insensitively, since the QML test doubles capitalise). The delegate keeps its stock content item (so dimming of disabled rows and the no-duplicate-text test still hold) and reserves right padding for the chip. Covered by `test_selectorLabelsOnlyExceptions` in `tst_BenchmarkWorkspace.qml`.
