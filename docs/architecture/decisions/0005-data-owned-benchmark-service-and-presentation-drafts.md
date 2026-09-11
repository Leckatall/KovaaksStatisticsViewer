---
status: accepted
date: 2026-09-10
---

# 0005: Own accepted benchmarks in a data service, editing in presentation, and profile reconciliation in the application layer

## Context

[ADR 0003](0003-profile-independent-benchmark-repository-and-application-owned-state.md) split the
benchmark "store" role across two layers: a Qt data-layer repository that knew only files and schema,
and an application-layer `BenchmarkLibraryService` that owned the accepted in-memory library, its
revision, the single active draft (baseline, dirty state, validation), and — later — scenario
reconciliation against the profile.

That single class accreted five responsibilities that belong to three different layers: file and JSON
mechanics, accepted-state authority, the profile join, temporary editor state, and Qt-free
publication. The accepted benchmark library is structurally the same kind of thing as the accepted
run profile, which `ProfileService` owns in the data layer over an injected serializer; hosting the
benchmark equivalent one layer up was the anomaly, not the rule.

## Decision

The store role is split by responsibility rather than by "files vs. everything else":

- **`BenchmarksService` (`src/data/`, Qt-free)** owns the accepted in-memory benchmark library, its
  monotonic in-process revision, refresh diagnostics, and coherent `onChanged` publication. It
  depends on an injected `IBenchmarkStore` and has no dependency on `IProfileService`, presentation,
  QML, or Qt filesystem APIs. It is analogous to `ProfileService`.
- **`IBenchmarkStore` / `BenchmarkStore` (`src/qt_data/`)** owns benchmark-directory enumeration,
  JSON encoding/decoding, schema classification, deterministic ordering, content digests, atomic
  replacement, conflict-aware deletion, and I/O diagnostics. It is stateless with respect to accepted
  application data: it reports outcomes; `BenchmarksService` alone decides whether an outcome changes
  accepted memory. The KovaaK's-specific `FileService` is unchanged and unrelated.
- **`BenchmarkResolutionUseCase` (`src/app/`)** is the sole owner of the join between accepted
  benchmark definitions and profile scenario identity. It subscribes to `IBenchmarksService` and
  `IProfileService`, publishes a derived `BenchmarkResolutionSnapshot` (catalogue, per-entry
  resolution results, automatic-mapping diagnostics), and submits automatic unique-name mappings as
  complete token-addressed replacement batches through `IBenchmarksService`. It performs no
  filesystem operations and guards against re-entrant callback execution so notification and write
  ordering stay deterministic.
- **`BenchmarkManagerViewModel` (`src/ui/presentation/`)** owns the editable working copy, its
  accepted baseline and admitted edit token, local dirty comparison, incomplete intermediate shapes,
  validation, and Save/Discard prompts. Structural mutations are pure `domain::BenchmarkEditor`
  operations over the caller-owned `domain::Benchmark`. Closing the manager discards an unsaved
  working copy; reopening creates a new one from accepted state.
- **`BenchmarkManagerUseCase` (`src/app/`)** composes accepted-library and resolution state for
  presentation, issues editor seeds, holds a single lightweight edit lease (benchmark id only),
  delegates whole-benchmark save/delete, and resolves a supplied working copy statelessly. It
  retains no working copy and exposes no individual editor mutation.

An open editor registers its benchmark id as an edit lease with the resolution use case, which keeps
deriving that benchmark's resolution but defers its automatic persistence until the lease is
released; release reconciles against the latest accepted definition and token.

This decision governs where accepted benchmark state, persistence mechanics, temporary editor state,
and the profile join live. It does not change the benchmark JSON schema, domain validation or
evaluation rules, startup/refresh behavior, incomplete-save rules, optimistic-conflict semantics, or
tracking behavior.

## Consequences

- The accepted benchmark library follows the same ownership pattern as the accepted run profile: a
  Qt-free data service over an injected persistence port.
- No application component owns the accepted library or performs benchmark filesystem operations; no
  data component depends on profile or presentation state; no presentation component can mutate
  accepted state without a service acceptance operation.
- An unsaved working copy survives explicit refresh but is never silently rebased; presentation
  surfaces a stale accepted baseline instead.
- Background automatic mapping never modifies the benchmark currently leased for editing; releasing
  the lease reconciles against the latest accepted state.
- One reconciliation batch produces at most one accepted-library publication; persistence conflicts
  and failures never mutate accepted memory.
- `BenchmarkManagerUseCase`'s constructor gains `IBenchmarksService`, `IBenchmarkResolutionUseCase`,
  and `IPlaylistReader`; `App::App()` constructs store → accepted service → resolution → manager and
  tracking before the initial profile load.

## Alternatives considered

- **Keep ADR 0003's application-owned `BenchmarkLibraryService`.** Rejected: it concentrates
  data-layer mechanics, accepted-state authority, the profile join, and editor state in one class one
  layer above where the equivalent run-history authority lives, and makes an application-initiated
  background reconciliation write visible to the user as a Save conflict behind an open draft.
- **A unified data-layer store owning persistence and the working copy, mirroring
  `SeriesConfigStore`.** Rejected for the same reason ADR 0003 rejected it: it places editor state
  and its reconciliation coupling below the layer that owns the profile join.
- **Rely only on optimistic conflicts instead of an edit lease.** Rejected: a profile callback could
  rewrite accepted state behind an open working copy and turn an application-initiated background
  update into a user-visible Save conflict.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [ADR-0003: Profile-independent benchmark repository and application-owned state](0003-profile-independent-benchmark-repository-and-application-owned-state.md) — superseded by this ADR
- [ADR-0004: Route view-model interactions through use cases](0004-route-view-model-interactions-through-use-cases.md)
- [Scenario benchmark tracking design](../../features/benchmarks/design.md)
