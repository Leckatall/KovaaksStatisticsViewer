---
type: internal
area: Architecture
---
Moved the benchmark-library snapshot DTOs (`BenchmarkLibrarySnapshot`, `BenchmarkFileEntry`, `LoadedBenchmark`, `ProblemBenchmark`, `BenchmarkFileProblem`) out of `data/interfaces/i_benchmark_repository.h` into a new `contracts/benchmark_library_snapshot.h` in `ksv_contracts`.
`IBenchmarkLibraryService` (the presentation contract) now includes only that header instead of the data-layer port header, so a UI translation unit can no longer `#include`-reach `IBenchmarkRepository` through it.
`i_benchmark_repository.h` includes the new contracts header for its `scan()` result — an upward include that matches the existing `ksv_data`/`ksv_qt_data` → `ksv_contracts` link edge; the repository port and its write/delete result types stay in place.
`benchmark_library_service.h` now includes `i_benchmark_repository.h` directly rather than getting `IBenchmarkRepository` transitively through the contract header.
