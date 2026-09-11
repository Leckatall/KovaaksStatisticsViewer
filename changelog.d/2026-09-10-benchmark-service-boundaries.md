---
type: changed
area: Benchmarks
---
Restructured benchmark ownership along the layer boundaries recorded in ADR 0005: the accepted
benchmark library moved into a Qt-free data-layer `BenchmarksService` over an injected
`IBenchmarkStore`; the profile/benchmark join moved into a dedicated application
`BenchmarkResolutionUseCase`, exposed through `IBenchmarkResolutionUseCase` snapshots and an edit
lease; and the editable working copy moved into `BenchmarkManagerViewModel` over a pure
`domain::BenchmarkEditor`. The editor performs structural mutations and validation on a caller-owned
`domain::Benchmark` using a caller-supplied id factory, while `BenchmarkManagerUseCase` composes the
data and resolution services, issues `BenchmarkEditorSeed`s, and accepts complete benchmark values.
`BenchmarkResolutionSnapshot` carries the derived scenario catalogue, per-benchmark resolutions,
and automatic-write diagnostics without retaining accepted benchmark copies. The old
application-owned `BenchmarkLibraryService`, `IBenchmarkRepository` alias, and repository-name alias
headers are gone; `BenchmarkTrackingUseCase` and `App::App()` now consume the replacement services.
Domain mutation coverage moved to `benchmark_editor_test.cpp`, and `tests_support` gained a fake for
the resolution seam. Startup, explicit refresh, incomplete saves, automatic unique-name mapping,
optimistic conflicts, diagnostics, and tracking behave as before.
