---
status: accepted
date: 2026-09-07
---

# 0002: Embed a stable identity in every benchmark definition element

## Context

Benchmark definitions are KSV-owned, human-readable, independently versioned JSON files that users
edit over time — renaming tiers, reordering the ladder, regrouping scenarios, and entering per-tier
thresholds.

The domain model fixes identity as an opaque value independent of names and order.
[`OpaqueId<Tag>`](../../../src/domain/benchmarks/benchmark_ids.h) is `{ std::string value; operator<=> }`
with distinct `BenchmarkId`, `TierId`, `GroupId`, and `ScenarioEntryId` tags. Every struct in
[`benchmark.h`](../../../src/domain/benchmarks/benchmark.h) — `Tier`, `Subcategory`, `Category`,
`Benchmark` — carries its `*Id`, and `Threshold` references a `TierId` rather than a ladder position.
The codec in [`benchmark_repository.cpp`](../../../src/qt_data/benchmark_repository.cpp) serializes and
decodes elements by these IDs, and `BenchmarkFileEntry` in
[`benchmark_library_snapshot.h`](../../../src/app/contracts/benchmark_library_snapshot.h) keys a managed
file by its `filename`, which is independent of the embedded `BenchmarkId`.

Because thresholds and later user selections reference elements, any identity derived from a mutable
attribute (name, order, filename) would let an ordinary edit silently retarget a threshold or a
selection.

## Decision

Every benchmark and every mutable element within a definition (tiers, groups, subcategories, scenario
entries) is identified by an immutable embedded UUID persisted in the JSON document. Names, ordering,
and the managed filename are display or addressing concerns and never constitute identity. New IDs are
generated only for newly created elements or by a future versioned migration; existing IDs are never
regenerated on rename, reorder, regroup, refresh, or save.

This decision governs the persistent identity contract of benchmark definition files. It does not
govern filename generation, whether a display-name change also renames the managed file, or how IDs are
generated at runtime.

## Consequences

- Rename, reorder, and regroup operations cannot retarget thresholds or selections, and diagnostics can
  point at a stable element.
- The embedded identity is a durable persistence and compatibility contract that will be expensive to
  reverse; it constrains the on-disk shape of every future schema version.
- JSON documents are more verbose than a name- or position-keyed format.
- Duplicate embedded IDs and dangling references must be rejected as invalid rather than repaired
  heuristically, because a heuristic repair could itself retarget thresholds or selections.

## Alternatives considered

- **Filename, display name, or array position as persistent identity.** Rejected because rename and
  reorder operations could silently retarget selection or thresholds.

## Links

- [Scenario benchmark tracking design](../../features/benchmarks/design.md)
