# Architecture Decision Records

This convention governs every Architecture Decision Record (ADR) in `docs/architecture/decisions/`.
An ADR preserves one architecturally significant, accepted decision:
the context that required it, the choice made, credible alternatives actually considered, and the consequences accepted with that choice.

The ADR files are the authoritative decision log. Do not maintain a duplicate index.

## Scope

Record a decision when it materially affects at least one of:

- major system structure, responsibilities, ownership, or relationships;
- cross-layer or public interfaces;
- persistent data, compatibility, migration, or recovery;
- important quality attributes such as performance, reliability, security, or testability;
- multiple features or implementation stages; or
- a direction that would be costly or risky to reverse.

One file records one decision. Separate independently significant choices even when they were discussed
or accepted together. Proposed, unresolved, and merely recommended choices are not ADR files.

Two choices are one decision only when neither could sensibly be reversed without the other. If one could be undone while the other stands, they are separate ADRs. Two signs that a record bundles more than one decision: its title needs "and" to join separate choices, or one of its alternatives applies to only part of what was decided.

Applying an accepted decision to a new case is not a new decision and gets no ADR. Departing from one is a decision, recorded as an amending or superseding ADR.

An ADR records what was chosen and why. The Architecture Description in `docs/architecture/` records what exists. Component inventories, per-class responsibilities, signatures, and wiring belong in the Architecture Description, which is kept current; an ADR that carries them goes stale, because its body is not revised.

An ADR body is a historical record. After acceptance, do not rewrite its decision text to describe a
later choice. Corrections that do not change meaning (typos, broken links, clarifying wording) are
permitted. Record a changed decision in a new amending or superseding ADR. Anything else that needs saying about an ADR after acceptance goes in its `## Amendments` section, which is the only part of an accepted ADR that grows.

Never delete an ADR, including one that is badly written or records more than one decision. Replace it through supersession, which keeps its identifier and its links resolvable.

The absence of an ADR does not imply that no decision exists. The log can be incomplete, especially
when ADRs are introduced retrospectively.

## Location, names, and identifiers

Store ADRs in `docs/architecture/decisions/` with filenames of the form
`NNNN-kebab-case-title.md`, beginning with `0001`. Use the same identifier in the heading:

```markdown
# NNNN: Decision title
```

A title names the one choice in a short phrase, about ten words at most, that reads naturally when the ADR is cited in a sentence. It does not list components or say who owns what; that is the Decision's job. A title that needs "and" or a comma-separated list is a sign that the record bundles more than one decision (see [Scope](#scope)).

The next identifier is one greater than the highest numeric filename prefix already present in that
directory. Never reuse an identifier. Resolve the next identifier again immediately before writing so
that a stale provisional number cannot be persisted.

Identifiers describe recording order only. They do not assert the chronological order in which the
underlying choices were originally made.

## Metadata

Every ADR begins with:

```yaml
---
status: accepted
date: YYYY-MM-DD
---
```

Only `accepted`, `superseded`, and `deprecated` are valid statuses:

- `accepted`: the decision is in force, as qualified by the ADR's `## Amendments` section.
- `superseded`: another ADR has replaced the decision.
- `deprecated`: the decision is withdrawn and nothing replaces it. It is no longer in force at all; the word does not mean "still valid but discouraged".

Write a new ADR only as `accepted`, and only after
the decision maker explicitly confirms the synthesized decision. A status describes the standing of the decision, not the quality of the record, so there is no status for an invalid or poorly written ADR.

`date` is the date the ADR is recorded, including for ordinary and retrospective ADRs. It is not an
assertion about when the underlying choice was first made.

Add the following fields only when they apply:

```yaml
retrospective: true
supersedes:
  - NNNN
superseded-by: MMMM
amends:
  - NNNN
```

- `retrospective: true` means the ADR documents a choice that predates the record. It does not claim
  that the original decision date is known.
- `supersedes` lists every earlier ADR fully replaced by this ADR.
- `superseded-by` names the accepted ADR that fully replaced this ADR. When the record was split, it names the primary replacement (see [Supersession](#supersession)). It does not change if that ADR is later superseded itself; follow the chain to reach the decision in force.
- `amends` lists every earlier ADR whose decision this ADR changes in part. Each listed ADR stays `accepted` and carries a matching entry in its `## Amendments` section.

Use zero-padded, four-digit identifiers in relationship fields.

## Body

Use the following sections in this order:

The first four sections are required, including in retrospective ADRs. `## Links` is optional, and `## Amendments` appears only after acceptance. When no credible alternative
is supported, retain `## Alternatives considered` and state concisely that available evidence does not
preserve any; do not populate it with conjecture.

### `## Context`

Describe the problem, constraints, and forces that made a decision necessary. Clearly distinguish
facts supported by durable evidence from the decision maker's recollection, bounded inference, or
unknown history.

### `## Decision`

State the accepted choice directly and define its material scope. The wording must make clear what the
decision governs and, when needed, what it does not govern.

Open with the choice itself, in full sentences, before any supporting detail. State it as a rule about responsibilities, ownership, layers, or interfaces, at the level a reader needs in order to tell whether a later change honours or breaks the decision. Name a component only where the decision is about that component.

Test each statement: if a rename or refactor that keeps the decision intact would make the statement false, it is design detail and belongs in the Architecture Description. A Decision section that reads as a list of components and what each one does describes a design; the reader should instead be able to finish the sentence "we chose to …" from its opening lines.

### `## Consequences`

Record important positive, negative, and neutral effects. Include meaningful obligations, risks, and
trade-offs without turning the ADR into an implementation plan.

Every decision costs something: added components, indirection, coordination state, or an option closed off. State at least one such cost; a list made only of guarantees is incomplete. Do not record constructor signatures, construction order, or other mechanics of carrying the decision out.

### `## Alternatives considered`

Include only credible alternatives that were actually considered, with concise reasons they were not
selected. Omit unsupported alternative details rather than reconstructing a more complete-looking history.
Rejected alternatives remain in this section; they do not become separate rejected ADRs.

### `## Links`

Include this section only when durable links add useful context, such as related ADRs, Architecture
Description sections, software design documents, or issues.

### `## Amendments`

Never part of a new ADR. Add this section, as the last section, the first time something must be said about the ADR after acceptance. Each entry is a list item that begins with the date it was added:

```markdown
## Amendments

- 2026-12-01: `FooViewModel` is now `BarViewModel`; the exemption is unchanged.
- 2027-02-14: Amended by [ADR-0012](0012-exempt-capture-view-model.md), which exempts the capture view model from this rule.
```

The section is append-only. Never edit or remove an entry; correct a wrong entry with a later one. Keep each entry to a sentence or two. An entry may qualify the decision or update what it points at, but must never contradict the Decision text. [Amendments](#amendments) defines what an entry may record.

No implementation-evidence section is required. Keep the record lightweight and focused on the
decision.

## Retrospective ADRs

A retrospective ADR records the date it is created. Never estimate the original decision date. Before
writing it, the decision maker must explicitly confirm that the existing choice remains accepted and
is architecturally significant.

Include original rationale, chronology, and alternatives only when supported by durable evidence or
the decision maker's explicit recollection. Current implementation can establish what exists; it does
not by itself establish why the choice was made or that the choice is accepted. Omit unsupported
history rather than presenting conjecture as fact.

## Supersession

Use full supersession only when a newly accepted ADR replaces the complete material decision in an
earlier ADR. Apply the relationship as one coordinated change:

1. Add every fully replaced identifier to the new ADR's `supersedes` list.
2. Change each replaced ADR's `status` to `superseded`.
3. Add `superseded-by: NNNN` to each replaced ADR, naming the new accepted ADR.
4. Leave every replaced ADR's historical body unchanged.

Supersession usually records a changed decision. It is also the way to re-record an unchanged decision whose ADR bundled several choices or was written as a design description. In that case the new ADR's Context states that the decision itself is unchanged and that the earlier record is replaced for form, so a reader does not look for a change of direction. The new ADR carries the current date and `retrospective: true`.

When the earlier record is split into several ADRs, record the remaining choices first as ordinary ADRs, then record the primary one, which carries the earlier record's main choice, as the superseding ADR. The primary ADR's `## Links` section names every other ADR from the split, so the complete replacement is reachable from the superseded record through `superseded-by`.

If a new decision changes only part of an earlier ADR, do not mark the earlier ADR as superseded. Amend it.

## Amendments

A reader takes an `accepted` ADR to be in force as its Decision and its `## Amendments` section together state. Everything that changes how an accepted ADR should be read is therefore recorded in that ADR's `## Amendments` section, so that it cannot be missed. There are three kinds of entry.

**A correction of a stale reference.** Something the ADR names was renamed, moved, or removed, or a named exception now conforms to the rule. The entry states the fact and, where one exists, the current name. No new ADR is needed.

**A minor adjustment.** The change is a choice, but it does not itself meet the significance threshold in [Scope](#scope): for example, an exempted class is split in two and both halves keep the exemption for the original reason. The entry states the adjustment in a sentence. No new ADR is needed. An adjustment that needs a paragraph of justification is not minor.

**A pointer to an amending ADR.** The change meets the significance threshold: it carves out a new exception, narrows the decision's scope, or otherwise alters part of it while the central choice stands. Record the change as a new ADR with the full body, and apply the relationship as one coordinated change:

1. Add the earlier ADR's identifier to the new ADR's `amends` list.
2. Add a dated entry to the earlier ADR's `## Amendments` section that links the new ADR and states in one sentence what it changes.
3. Leave the earlier ADR's status and the rest of its body unchanged.

To choose between the first two kinds and the third, ask whether the change would be recorded as an ADR if it stood alone. When the central choice itself changes, however slightly, supersede instead of amending.

## Deprecation

Deprecate an ADR when its decision is withdrawn and no ADR replaces it: for example, an amending ADR granted an exemption that no longer exists. Apply it as one coordinated change:

1. Change the ADR's `status` to `deprecated`.
2. Add a dated entry to its `## Amendments` section stating why the decision no longer applies.
3. If the ADR amends others, add a dated entry to each of their `## Amendments` sections stating that the amendment is withdrawn.
4. Leave the rest of the body unchanged.

When the withdrawal is itself a significant decision, such as abandoning a rule the codebase was built around, record that decision as a new ADR and supersede instead.
