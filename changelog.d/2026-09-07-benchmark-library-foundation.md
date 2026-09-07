---
type: added
area: Benchmarks
---
Slice 1 of scenario benchmark tracking: Qt-free benchmark model + completeness validator (`ksv_domain`), a filesystem `IBenchmarkRepository` (`ksv_qt_data`) with JSON codec, `schemaVersion` gating, content-digest-guarded atomic writes, and per-file classification, plus a minimal `BenchmarkLibraryService` (`ksv_app`) owning the accepted snapshot + refresh, wired in `App::App()`. No user-facing surface yet.
