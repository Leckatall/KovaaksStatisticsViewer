---
type: changed
area: Benchmarks
user: In an incomplete benchmark where two rows are linked to the same played scenario, total playtime now counts that scenario's runs once for each row.
---
`ScenarioEntry::hash` and `additionalHashes` are replaced by one `hashes` list (unique, non-empty, user order) and `mappedScenarioHashes` is gone. The stored format is unchanged. `ScenarioResolution` no longer carries hashes; `BenchmarkTrackingUseCase::evaluate` reads them from the loaded definition and the evaluator from each `ScenarioEntry`. `evaluateBenchmark` builds each entry's history in one pass over `runFacts`, computes each threshold ladder once and sums total playtime from the per-entry figures. Linked-hash runs with equal start times now keep profile order.
