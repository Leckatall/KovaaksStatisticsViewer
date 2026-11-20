---
type: internal
area: Benchmarks
---
Removed `BenchmarkProjection`, `ScenarioProjection`, `GroupProjection`, `evaluateBenchmark`, `toProjection`, `thresholdLadder` and `normalizedTierValue`. The progress grid and tracking view model read the evaluated `Benchmark`.
Removed `BenchmarkWorkspaceSnapshot::selectedLoaded`, `BenchmarkWorkspaceSnapshot::projection` and `IBenchmarkTrackingUseCase::projection()`. The tracking use case evaluates each benchmark once and publishes `snapshot.benchmark` and `visiblePlaytime`. `benchmark()` exposes the snapshot's evaluated benchmark.
