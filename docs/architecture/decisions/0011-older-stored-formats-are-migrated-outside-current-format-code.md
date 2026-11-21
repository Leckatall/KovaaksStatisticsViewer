---
status: accepted
date: 2026-11-21
---

# 0011: Older stored formats are migrated outside current-format code

## Context

The application reads several bodies of persisted state whose stored formats have changed over time: the run profile, benchmark definition files, the graph configuration document, and individual settings keys. Each still accepts data written in some earlier format.

The run profile already keeps the two apart. Its retired version 3 is decoded in a separate migration unit with its own retired schema ([`profile_v3_migrator.h`](../../../src/data/formats/protobuf/migration/profile_v3_migrator.h)), which the serializer reaches only through `IProfileMigrator` after detecting a non-current version. The current profile decoder in [`profile_serializer.cpp`](../../../src/data/formats/protobuf/profile_serializer.cpp) contains no version branches.

The other stores handle older formats inside the code that reads the current one:

- Benchmark definitions: the stored version is threaded through the whole decode tree in [`benchmark_store.cpp`](../../../src/qt_data/benchmark_store.cpp) so that one entry decoder can branch on it.
- Graph configuration: [`graph_configuration_codec.cpp`](../../../src/qt_data/graph_configuration_codec.cpp) maps the document version onto a second version numbering for the series section, and [`series_config_codec.cpp`](../../../src/qt_data/series_config_codec.cpp) passes older versions into the current decoder as boolean flags.
- Settings: reads of superseded keys are methods on `ISettingsService`, and some current accessors in [`settings_service.cpp`](../../../src/qt_data/settings_service.cpp) fall back to an earlier key inline.

As a result, dropping support for an older graph configuration or benchmark version means editing the decoder that reads the current one. On 2026-11-21 the developer stated that migration code should be clearly separated from the rest of the reading and writing code, and that this means current-format code having no knowledge of older formats, not merely living in different files.

## Decision

Code that reads or writes a body of persisted state knows only that state's current stored format. Every older format the application still reads is handled by migration code kept apart from it. Current-format code refers to older formats at exactly one point: where it detects that stored data is not current and hands it to migration.

A stored format is any persisted representation that has changed incompatibly. That includes a versioned file or document, and a settings key that has been renamed or replaced or whose meaning has changed.

This decision does not govern how a migration is carried out (upgrading older data step by step to the current stored format, or decoding it directly into the in-memory model), when migrated data is written back, which older formats are supported or for how long, or how unreadable data is rejected or quarantined.

The existing handling of older formats inside current-format code for benchmark definitions, graph configuration, and settings keys that replaced earlier keys is nonconforming architectural debt. It is neither a sanctioned exception nor precedent, and recording this decision does not require it to be migrated now.

## Consequences

- Dropping an older format means deleting its migration code and its entry at the hand-off point; the current-format code does not change.
- Current-format code and its tests describe a single format, and tests for older formats sit with the migration code.
- An incompatible format change adds a migration unit rather than another branch in current-format code.
- Every incompatible change needs separate migration code, even a trivial one such as a renamed field or settings key, and that code may restate structure the current decoder already knows.
- The three nonconforming stores need restructuring before they comply. Until then, changes to them must not add further inline handling of older formats; the debt does not weaken the rule for new work.
- Write-back timing, how long older formats are supported, and rejection behaviour remain decided per store.

## Alternatives considered

- **Keep handling older formats inline in current-format code**, as three stores do now. Rejected: current-format code grows with every version, and dropping a version means editing it.
- **Separate migration into its own files while the current decoder still branches on version.** Rejected: the files are apart, but the current-format code still has to know about older formats.
- **Prescribe one migration mechanism or framework for every store.** Rejected: protobuf and JSON stores migrate naturally in different shapes, and a shared framework is more machinery than four stores need.

## Links

- [ADR-0004: Route view-model interactions through use cases](0004-route-view-model-interactions-through-use-cases.md), precedent for recording existing violations as debt
- [ADR-0010: One data-layer authority for each body of persisted state](0010-one-data-layer-authority-for-each-body-of-persisted-state.md), which does not govern stored formats
- [Run history data view](../views/run-history-data.md), [Graph configuration view](../views/graph-configuration.md) and [Benchmark definitions view](../views/benchmark-definitions.md), which describe the current migration behaviour
