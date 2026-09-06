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

An ADR body is a historical record. After acceptance, do not rewrite its decision text to describe a
later choice. Corrections that do not change meaning (typos, broken links, clarifying wording) are
permitted. Record a changed decision in a new related, amending, or superseding ADR.

The absence of an ADR does not imply that no decision exists. The log can be incomplete, especially
when ADRs are introduced retrospectively.

## Location, names, and identifiers

Store ADRs in `docs/architecture/decisions/` with filenames of the form
`NNNN-kebab-case-title.md`, beginning with `0001`. Use the same identifier in the heading:

```markdown
# NNNN: Decision title
```

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

Only `accepted` and `superseded` are valid statuses. Write a new ADR only as `accepted`, and only after
the decision maker explicitly confirms the synthesized decision.

`date` is the date the ADR is recorded, including for ordinary and retrospective ADRs. It is not an
assertion about when the underlying choice was first made.

Add the following fields only when they apply:

```yaml
retrospective: true
supersedes:
  - NNNN
superseded-by: MMMM
```

- `retrospective: true` means the ADR documents a choice that predates the record. It does not claim
  that the original decision date is known.
- `supersedes` lists every earlier ADR fully replaced by this ADR.
- `superseded-by` names the accepted ADR that fully replaced this ADR.

Use zero-padded, four-digit identifiers in relationship fields.

## Body

Use the following sections in this order:

The first four sections are required, including in retrospective ADRs. When no credible alternative
is supported, retain `## Alternatives considered` and state concisely that available evidence does not
preserve any; do not populate it with conjecture.

### `## Context`

Describe the problem, constraints, and forces that made a decision necessary. Clearly distinguish
facts supported by durable evidence from the decision maker's recollection, bounded inference, or
unknown history.

### `## Decision`

State the accepted choice directly and define its material scope. The wording must make clear what the
decision governs and, when needed, what it does not govern.

### `## Consequences`

Record important positive, negative, and neutral effects. Include meaningful obligations, risks, and
trade-offs without turning the ADR into an implementation plan.

### `## Alternatives considered`

Include only credible alternatives that were actually considered, with concise reasons they were not
selected. Omit unsupported alternative details rather than reconstructing a more complete-looking history.
Rejected alternatives remain in this section; they do not become separate rejected ADRs.

### `## Links`

Include this section only when durable links add useful context, such as related ADRs, Architecture
Description sections, software design documents, or issues.

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

## Supersession and amendments

Use full supersession only when a newly accepted ADR replaces the complete material decision in an
earlier ADR. Apply the relationship as one coordinated change:

1. Add every fully replaced identifier to the new ADR's `supersedes` list.
2. Change each replaced ADR's `status` to `superseded`.
3. Add `superseded-by: NNNN` to each replaced ADR, naming the new accepted ADR.
4. Leave every replaced ADR's historical body unchanged.

If the new decision changes only part of an earlier ADR, document the changed scope as an amendment or
related decision. Link the records through their optional `## Links` sections. Do not mark the entire
earlier ADR as superseded.
