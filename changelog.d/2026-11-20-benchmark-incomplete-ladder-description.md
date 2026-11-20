---
type: changed
area: Benchmarks
user: On the benchmark progress grid, a scenario whose tier thresholds are missing or out of order now describes its score as "thresholds incomplete" instead of guessing a tier from the usable part.
---
`progressDescription` guards once on `ScenarioBenchmarkEvaluation::ladder()` and reads tier positions from the ladder, replacing the per-tier missing and non-increasing threshold checks.
