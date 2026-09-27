---
type: internal
area: Architecture
---
`App`'s full injected constructor now requires `IStatsCsvParser` and `IBenchmarkStore` with no null fallbacks, matching `ISeriesConfigStore`; the real `StatsCsvParser` and `AppDataLocation/benchmarks` `BenchmarkStore` are built only in the delegating `App(settings, decoder)` constructor, which no longer takes a parser.
Previously `benchmarkStore` defaulted to `nullptr` and was replaced in the body, so every integration fixture that omitted it silently shared one test-mode AppData `benchmarks` directory across tests and runs.
`integration::TestEnv` now owns a `StatsCsvParser` and a `BenchmarkStore` over its own `QTemporaryDir`; `FirstRunUiTest` and `ksv_gallery` supply temp-dir stores too.
