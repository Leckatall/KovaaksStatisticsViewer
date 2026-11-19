---
type: changed
area: Benchmarks
---
`BenchmarkTrackingUseCase` re-evaluates only on `IBenchmarkResolutionUseCase::onChanged`, which now documents that it fires after every benchmark-library or profile change. `BenchmarkResolutionUseCase::runReconcilePass` builds the scenario catalogue once per pass instead of once per loaded benchmark.
