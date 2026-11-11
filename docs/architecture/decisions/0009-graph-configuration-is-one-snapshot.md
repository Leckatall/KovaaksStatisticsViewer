---
status: accepted
date: 2026-11-11
retrospective: true
---

# 0009: Graph configuration is one snapshot

## Context

This record re-states one of the choices bundled in [ADR 0006](0006-unified-graph-configuration-and-presentation-drafts.md). The decision itself is unchanged; ADR 0006 is replaced for form, because it recorded several choices in one file and described them as a component design. Where committed graph configuration and its editing draft live is the other choice in ADR 0006, and is now stated by the primary record.

ADR 0006 records the situation before the choice. Graph Lines series and axes were one stored document; the Y-axis-label selector was a separate setting written directly from QML, so one settings visit had two independent persistence paths. The graph-configuration feature then added independent preferences for five graph types, changed either immediately from a per-graph popup or through one cancellable settings visit that also contains Graph Lines editing.

Two forces follow from that, both recorded in ADR 0006: a settings visit must isolate and then apply edits across every graph and Graph Lines at once, and graph preferences must survive an upgrade alongside the existing series document.

The [graph-configuration design](../../features/graph-configuration/design.md) lists "a unified versioned configuration document" among the choices the developer adopted.

## Decision

All graph configuration is one committed snapshot: every graph type's preferences together with the Graph Lines series, expressions, axes and axis-label selection. It is stored as one versioned document.

The snapshot changes only by being replaced as a whole, after the complete candidate has been validated. A change addressed to a single graph is still a validated replacement of the whole snapshot. An invalid candidate changes nothing.

Each accepted replacement is published once. No observer sees a state in which some graphs have been updated and others have not.

This governs the unit of graph configuration: what is committed, validated, stored and published together. It does not govern which layer owns the committed snapshot or the editing draft (the primary record), the durability guarantee of the underlying write, or history-range and rendering behaviour.

## Consequences

- A save is atomic across graphs, and a settings visit that touches several graphs and Graph Lines applies as one change.
- Invalid input on any one graph blocks the whole save, including edits to other graphs that were valid.
- One document couples upgrades. A migration for graph preferences must carry the Graph Lines series, axes and identifier allocation through intact, and an invalid preference section must not discard otherwise valid series.
- There is one persistence path for a settings visit. The axis-label selection is part of the snapshot and is no longer written separately.
- A quick change to one graph costs a whole-snapshot validation and replacement, and notifies every consumer of graph configuration, not only the graph that changed.
- There is one working copy per settings visit and no protocol for concurrent editors or conflicting saves. This holds only while the settings visit is modal.

## Alternatives considered

Available evidence does not preserve an alternative to the unified snapshot that was considered. The alternative ADR 0006 records concerns where the editing draft lives, and is carried by the primary record.

## Links

- [ADR-0006: Own committed graph configuration in a data service, drafts in presentation, behind a complete-snapshot use case](0006-unified-graph-configuration-and-presentation-drafts.md), the record this one partly replaces
- [ADR-0010: One data-layer authority for each body of persisted state](0010-one-data-layer-authority-for-each-body-of-persisted-state.md), the primary record of the split
- [Graph configuration design](../../features/graph-configuration/design.md)
- [Graph configuration view](../views/graph-configuration.md), which describes the components that implement this
