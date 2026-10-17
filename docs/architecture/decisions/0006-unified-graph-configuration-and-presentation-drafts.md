---
status: accepted
date: 2026-10-17
retrospective: true
---

# 0006: Own committed graph configuration in a data service, drafts in presentation, behind a complete-snapshot use case

## Context

The graph-configuration feature gives five graph types (`run`, `dailyPlaytime`, `scenarioHistory`, `benchmarkRank`, `benchmarkPlaytime`) independent preferences, adjusted either immediately from a per-graph quick popup or through one cancellable settings visit that also contains Graph Lines editing.

The design basis recorded how configuration was owned before this decision:

- `SeriesConfigStore` combined persistence, the accepted series/axis data and a draft shadow in one object. Draft mutations changed what `getAll()` returned and emitted configuration callbacks before commit, and `GraphUseCase` consumed that same store, so an open draft was not isolated from the dashboard.
- The Graph Lines Y-axis-label selector wrote directly to a QML setting rather than to that draft, so one settings visit had two independent persistence paths.

The new feature added forces the store could not satisfy: a settings visit must isolate edits across every graph and Graph Lines at once, quick changes must take effect immediately, and graph preferences must survive an upgrade alongside the existing series document. [ADR 0005](0005-data-owned-benchmark-service-and-presentation-drafts.md) had already established an accepted data service with a presentation-owned working copy for benchmarks; the design treated it as a precedent rather than a mandate for all configuration.

## Decision

Graph configuration is owned as one committed snapshot, split by responsibility:

- **`GraphConfiguration`** (application contract values) is a single typed snapshot holding each graph type's preferences together with the existing Graph Lines series, expressions, axes, ID allocators and run-graph axis-label selection.
- **`GraphConfigurationService`** (`src/data/`, Qt-free, behind `IGraphConfigurationService`) is the sole authority over the committed snapshot and its in-process revision. It replaces the snapshot only as a whole, after validating the complete candidate, and publishes one change notification per accepted replacement. It holds no editor working copy.
- **`GraphConfigurationPersistence`** (`src/qt_data/`, behind `IGraphConfigurationPersistence`) owns the versioned JSON codec, schema migration, legacy import, quarantine and QSettings submission. It is stateless with respect to accepted data and is the single writer of the configuration document.
- **`IGraphConfigurationUseCase`** is presentation's only route to configuration: read the committed snapshot, subscribe to changes, save a complete candidate, or apply a quick patch addressed by graph type. Both writes validate before replacing anything, and an invalid candidate or patch changes nothing. The use case retains no working copy.
- **`SettingsViewModel`** owns the single working copy of a settings visit, its baseline, raw input, validation issues and dirty state. Graph Lines edits are transformations of that caller-owned copy. Graph consumers read only committed state.

This decision governs where committed graph configuration, its persistence mechanics and its editing draft live, and the shape of the application boundary between them. It does not govern benchmark definitions or drafts (ADR 0005), the durability guarantee of a QSettings submission, history-range or rendering behaviour, or the remaining direct service dependencies `SettingsViewModel` carries as debt under [ADR 0004](0004-route-view-model-interactions-through-use-cases.md).

## Consequences

- Uncommitted edits can no longer reach the dashboard; graph refreshes follow committed revisions and draft edits notify only the editor.
- Graph configuration follows the same ownership pattern as the run profile and the benchmark library: a Qt-free data service over an injected persistence port.
- Saves are atomic at the snapshot level: invalid input on any graph blocks the whole save, and no observer sees a partially updated set of graphs.
- Use-case and persistence ports can be replaced by fakes in tests, and `App`'s injected constructor takes the persistence port explicitly.
- Graph Lines editing changed its mutation and notification paths, and the store-owned draft is retired rather than kept as a second mutable authority.
- One unified document couples graph-preference upgrades to Graph Lines upgrades. Migrations must preserve series definitions, axes, allocator exhaustion and legacy selector choices, and an invalid preference section must not discard otherwise valid series.
- The document moves to schema version 4 but keeps its historical `graph/seriesConfigV1` QSettings key, and QML no longer persists the run-axis label selection itself.
- The window-modal settings dialog means there is at most one working copy, so no concurrent-editor or conflict protocol is required.

## Alternatives considered

- **Keep the store-owned shared draft in `SeriesConfigStore`.** Rejected: draft mutations were visible to the dashboard before Save, persistence, accepted state and editor state remained combined in one object, and it could not isolate a single visit spanning every graph's preferences.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [ADR-0004: Route view-model interactions through use cases](0004-route-view-model-interactions-through-use-cases.md)
- [ADR-0005: Own accepted benchmarks in a data service, editing in presentation, and profile reconciliation in the application layer](0005-data-owned-benchmark-service-and-presentation-drafts.md)
- [Graph configuration design](../../features/graph-configuration/design.md)
