---
status: accepted
date: 2026-11-11
retrospective: true
---

# 0008: No background writes to an entity under edit

## Context

This record re-states one of the choices bundled in [ADR 0005](0005-data-owned-benchmark-service-and-presentation-drafts.md). The decision itself is unchanged; ADR 0005 is replaced for form, because it recorded several choices in one file and described them as a component design.

The application writes to accepted benchmark definitions on its own initiative: when a profile change makes a scenario name uniquely matchable, the automatic mapping is written back to the stored definition. The user edits the same definitions through a working copy that is saved as a whole against the accepted version it was opened from.

These two writers collide. A background write that lands while a working copy is open changes the accepted version underneath it, so the user's next Save is refused as a conflict that the user did not cause and cannot understand. ADR 0005 records this as the force behind the choice.

## Decision

While an entity is open for editing, the application does not write to that entity's accepted state on its own initiative. Background writes to it are deferred until editing ends, and are then decided afresh against the accepted state as it stands at that moment.

The exclusion is enforced by the component that performs the background write, not by the editor. The editor's side only declares which entity is under edit and when that ends.

The exclusion covers writes only. State derived from the entity for display keeps being computed while it is under edit.

This governs application-initiated writes to accepted state that the user can also edit. It does not govern writes the user asks for, external changes to stored files, or how a save detects that its baseline has moved.

## Consequences

- An open working copy is never invalidated by the application's own background work. A conflict on Save always means something outside the application's background writers changed the entity.
- The application carries coordination state: which entity is under edit. It must be released on every path that ends an editing session, or background writes to that entity stay suppressed.
- Anything the background writer would have done to the entity under edit does not happen during the session. A save from the editor must therefore do that work itself, or the saved entity stays without it until editing ends.
- Ending an editing session can trigger a background write and a publication, so closing an editor is not free of side effects.
- Only one entity is excluded at a time. This is sufficient while editing is modal; concurrent editors would need the exclusion widened.

## Alternatives considered

- **Rely only on optimistic conflict detection.** Rejected in ADR 0005: a profile change could rewrite accepted state behind an open working copy and turn an application-initiated background update into a user-visible Save conflict.

## Links

- [ADR-0005: Own accepted benchmarks in a data service, editing in presentation, and profile reconciliation in the application layer](0005-data-owned-benchmark-service-and-presentation-drafts.md), the record this one partly replaces
- [ADR-0010: One data-layer authority for each body of persisted state](0010-one-data-layer-authority-for-each-body-of-persisted-state.md), the primary record of the split
- [Benchmark editing view](../views/benchmark-editing.md), which describes how the exclusion is implemented
