---
status: Superseded by ADR 0005
date: 2026-09-07
---

# 0003: Keep the benchmark repository profile-independent and own accepted state and the draft in the application layer

> **Superseded by [ADR 0005](0005-data-owned-benchmark-service-and-presentation-drafts.md).** The
> profile-independence of the persistence port is retained, but accepted-library ownership moved from
> an application-layer service into a Qt-free data-layer `BenchmarksService`, temporary editor state
> moved into presentation, and the profile join became a dedicated application use case. This record
> is kept for historical context.

## Context

Benchmark files can be valid, incomplete, invalid, unsupported, externally modified, or deleted
independently of the user's run history, and scenario resolution needs a single mutation authority.

The current split reflects this. The data-layer port
[`IBenchmarkRepository`](../../../src/data/interfaces/i_benchmark_repository.h) exposes only file and
schema operations — `scan()`, `write(definition, filename, expectedDigest)`,
`remove(filename, expectedDigest)`, and `managedDirectoryPath()` — and names no profile type. The
application-layer [`BenchmarkLibraryService`](../../../src/app/usecases/benchmark_library_service.cpp)
owns the accepted in-memory snapshot and its `revision()`, and is the only publisher of `onChanged`.

The contrasting precedent in the codebase is
[`ISeriesConfigStore`](../../../src/data/interfaces/i_series_config_store.h): a data-layer store that
itself carries the draft lifecycle (`beginDraft`, `commitDraft`, `discardDraft`, `hasPendingChanges`,
implemented in [`series_config_store.cpp`](../../../src/qt_data/series_config_store.cpp)). Mirroring it
would place the benchmark draft in the data layer.

## Decision

The benchmark "store" role is split across two layers. The data-layer repository knows only files and
schema and stays independent of the profile. The application-layer library service is the sole owner and
mutation authority for the accepted in-memory library, the single active draft (its baseline, dirty
state, and validation result), and — in a later slice — scenario reconciliation against the profile.

This decision governs where accepted benchmark state, the draft, and mutation authority live. It does
not govern the internal shape of the draft or the specific mutation commands.

## Consequences

- There is one writer for accepted benchmark state, which keeps reconciliation and draft/dirty
  coordination (refresh exclusion while dirty, deferred automatic-mapping writes) in one place.
- The repository stays a thin file port that is deterministically testable against a temporary
  directory with no profile dependency.
- This deliberately diverges from `SeriesConfigStore`. Recording it prevents a future contributor from
  "mirroring `SeriesConfigStore`" and moving the draft into the data layer — the same class of
  pattern-mirroring mistake that [ADR-0001](0001-application-interface-and-implementation-placement.md)
  exists to prevent.
- Profile-derived state cannot enter the repository, so name-to-hash resolution must be performed above
  it.

## Alternatives considered

- **Profile-aware parsing or ownership inside the repository.** Rejected because it would couple file
  admission and migration to mutable run state and create a second owner for scenario resolution.
- **A unified data-layer store owning both persistence and the draft, mirroring `SeriesConfigStore`.**
  Rejected because it would place the draft and its reconciliation coupling below the layer that owns
  the profile join, splitting mutation authority across layers.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [Scenario benchmark tracking design](../../features/benchmarks/design.md)
