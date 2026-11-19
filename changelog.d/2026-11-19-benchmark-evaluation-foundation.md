---
type: internal
area: Benchmarks
---
`BenchmarkDefinition` and its types now live in `benchmarks/benchmark_definition.h`, with read-only helpers `entries()`, `scenarioHashes()`, `ScenarioEntry::thresholdFor` and `BenchmarkDefinition::ladderFor` that preserve the existing threshold-admission rules. `benchmarks/benchmark.h` declares the evaluated `Benchmark`, `ScenarioBenchmarkEvaluation` and `GroupEvaluation` for the evaluator conversion. `RankBlocker` lives in `benchmark.h` and `AverageRankPoint` in `rank_history.h`.
`RunFact::timed()` extracts the numeric sample used by `ScenarioEvaluation` and `rollingPlaytime`. `ScenarioEvaluation` computes run count, personal best and its earliest occurrence, recent average and playtime from chronological samples. `UserProfile` delegates to the shared `rollingPlaytime` algorithm with the existing window and epoch rules.
`BenchmarkWorkspaceSnapshot` gained `benchmark` and `visiblePlaytime`, and `IBenchmarkTrackingUseCase` gained `benchmark()`. These are unpopulated until the tracking use case is converted.
