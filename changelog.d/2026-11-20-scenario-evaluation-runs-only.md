---
type: internal
area: Benchmarks
---
`ScenarioEvaluation::kRecentRuns` is an `int`, and the personal best relies on chronological input for its earliest-tie rule.
`ScenarioEvaluation::fromRuns` returns nothing for an empty history, so `ScenarioBenchmarkEvaluation::stats()` is optional and its fields are no longer individually optional.
