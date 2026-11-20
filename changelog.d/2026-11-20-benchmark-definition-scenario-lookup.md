---
type: internal
area: Benchmarks
---
`BenchmarkDefinition::scenarioLists()` and `BenchmarkDefinition::entry(id)` replace the `forEachScenarioList` and `findEntry` helpers that the editor and the resolution use case each kept a copy of. `entries()` is built on `scenarioLists()`, so all three share one canonical order.
