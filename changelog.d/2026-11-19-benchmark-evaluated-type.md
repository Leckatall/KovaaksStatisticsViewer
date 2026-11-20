---
type: internal
area: Benchmarks
---
`Benchmark` evaluates a definition by value from its completeness, resolutions, and chronological run facts. It owns scenario and group evaluations, so copies are independent. Incomplete benchmarks include the tree, per-scenario statistics, total and rolling playtime, and latest run day. Rank results and group satisfaction require a Trackable benchmark with at least one scenario.

Per-scenario statistics come from `ScenarioEvaluation`. Total playtime, rolling playtime, and latest run day use the same chronological sequence of Resolved-entry runs.
