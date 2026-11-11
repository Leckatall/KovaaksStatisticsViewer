---
status: accepted
date: 2026-11-11
retrospective: true
supersedes:
  - '0005'
  - '0006'
---

# 0010: One data-layer authority for each body of persisted state

## Context

This record re-states the ownership choice made twice before: for benchmarks in [ADR 0005](0005-data-owned-benchmark-service-and-presentation-drafts.md) and for graph configuration in [ADR 0006](0006-unified-graph-configuration-and-presentation-drafts.md). Both are replaced for form: each recorded several choices in one file and described them as a list of components and their responsibilities. No change of direction for benchmarks or graph configuration is intended.

The scope is wider than either earlier record, and that is deliberate. ADR 0005 decided the choice for the benchmark library, and ADR 0006 records that its design treated ADR 0005 "as a precedent rather than a mandate for all configuration". On 2026-11-11 the developer confirmed that the same choice also holds for the run profile and for settings, and that it should be recorded as one rule. Stating it once as a rule is the new element of this record.

The history behind the choice is in the two earlier records. [ADR 0003](0003-profile-independent-benchmark-repository-and-application-owned-state.md) had placed the accepted benchmark library in an application-layer service above a data-layer repository that knew only files and schema. ADR 0005 records what followed: one application class came to hold file and JSON mechanics, authority over accepted state, the join to the run profile, temporary editor state and publication, which are concerns of three different layers. It also records the comparison that settled it: the accepted benchmark library is the same kind of thing as the accepted run profile, which a data-layer service already owned over an injected serializer, and hosting the benchmark equivalent one layer up "was the anomaly, not the rule".

ADR 0006 records the same problem in a different place. One store combined persistence, accepted graph data and a draft, so an open draft changed what the dashboard read before it was saved.

## Decision

Each body of accepted persisted state has exactly one authority. The authority lives in the data layers, alone decides what is accepted, and publishes changes to it. Everything above reaches it only through a Qt-free interface.

Deciding what is accepted is kept apart from storing it. Where an authority holds accepted state in memory or can refuse a candidate, that logic is Qt-free and reaches storage through an injected persistence port, whose implementation carries the file, format and I/O mechanics and holds no accepted state of its own. An authority that only reads and writes stored values has nothing to keep apart, and may be the storage adapter itself.

An authority holds accepted state only. Where state is edited through a working copy, the working copy lives above the authority and reaches it only as a complete candidate.

An authority does not combine its state with another authority's. Where two bodies of accepted state must be combined, the join is owned by one component in the application layer, which reads both and writes back only through the authority it is changing.

This governs where accepted state, its persistence mechanics, unsaved edits and cross-authority joins live, for every body of persisted state the application treats as authoritative. It does not govern stored formats, validation rules, conflict semantics, what unit a given authority accepts and publishes, or which element inside the presentation layer holds a working copy or prompts the user.

## Consequences

- A new kind of persisted state has a known home and a known shape, and its authority can be tested without Qt and without a filesystem by faking the port.
- An authority that decides anything costs a service, a port and a port implementation, including where one class would have been enough to make it work.
- No application or presentation component can change accepted state except by submitting to its authority, and a persistence failure cannot leave memory and storage disagreeing about what is accepted.
- Unsaved edits live in presentation, because that is the only layer above the authority that holds user input. A working copy is discarded or saved as a whole; the authority never rebases it, so presentation has to detect and show that the accepted version it started from has moved.
- An edit in progress cannot reach anything that reads accepted state, so nothing outside the editor changes until a save is accepted.
- A join is a second, derived publication beside each authority's own. A reader that needs both makes two reads that are not guaranteed to describe the same moment, and the join's owner has to keep notification and write ordering deterministic.
- Because authorities do not combine each other's state, one body of state can be loaded, refreshed or migrated without the content of another.

## Alternatives considered

- **Keep the application-layer owner of accepted benchmark state from ADR 0003.** Rejected in ADR 0005: it concentrated data-layer mechanics, accepted-state authority, the profile join and editor state in one class, one layer above where the equivalent run-history authority lives.
- **One data-layer store owning both persistence and the working copy.** Rejected in ADR 0005 for benchmarks, for the reason ADR 0003 had already rejected it: it places editor state, and the coupling to the join that comes with it, below the layer that owns the join. Rejected again in ADR 0006 for graph configuration, where such a store already existed: its draft mutations were visible to the dashboard before Save, and it could not isolate one visit spanning every graph's preferences.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [ADR-0003: Profile-independent benchmark repository and application-owned state](0003-profile-independent-benchmark-repository-and-application-owned-state.md), superseded by ADR 0005
- [ADR-0004: Route view-model interactions through use cases](0004-route-view-model-interactions-through-use-cases.md)
- [ADR-0005: Own accepted benchmarks in a data service, editing in presentation, and profile reconciliation in the application layer](0005-data-owned-benchmark-service-and-presentation-drafts.md), replaced by this record
- [ADR-0006: Own committed graph configuration in a data service, drafts in presentation, behind a complete-snapshot use case](0006-unified-graph-configuration-and-presentation-drafts.md), replaced by this record
- [ADR-0008: No background writes to an entity under edit](0008-no-background-writes-to-an-entity-under-edit.md), the remaining choice from ADR 0005
- [ADR-0009: Graph configuration is one snapshot](0009-graph-configuration-is-one-snapshot.md), the remaining choice from ADR 0006
- [Benchmarks view](../views/benchmarks.md) and [Graph configuration view](../views/graph-configuration.md), which describe the components that implement this
