---
status: accepted
date: 2026-11-22
amends:
  - 0011
---

# 0013: Migrations upgrade stored data one version at a time

## Context

[ADR-0011](0011-older-stored-formats-are-migrated-outside-current-format-code.md) keeps older stored formats out of current-format code but does not govern how a migration is carried out. [ADR-0012](0012-migrations-produce-current-stored-data-never-the-in-memory-model.md) requires migration to produce data in the current stored format, which still leaves open whether each older version converts straight to the current format or passes through every version in between.

No store yet has more than one older version to migrate: the run profile migrates only its retired version 3 to the current version 4 ([`profile_serializer.cpp`](../../../src/data/formats/protobuf/profile_serializer.cpp)). Benchmark definition files are the first case: files at schema 1 and 2 will be upgraded 1→2 and 2→3 as stored JSON and then read by a reader that knows only schema 3.

## Decision

Older stored data is upgraded one version at a time, vN → vN+1 → … → current. Each migration step knows only its source and target formats. A new incompatible format adds one step from the previous format and leaves the existing steps unchanged.

This applies to every body of persisted state that ADR-0011 governs. It does not govern whether or when migrated data is written back, which older formats are supported or for how long, or how unreadable data is rejected.

## Consequences

- Adding a format never touches older steps, and dropping support for the oldest format means deleting its first step.
- A failure belongs to one step and is reported to the user as `Migration from version x to version x+1 failed: <reason>`, the wording every migration uses.
- Data several versions old passes through every intermediate step, which is slower than one direct conversion, and whatever an intermediate format cannot express does not survive to the current format.
- Every intermediate format, such as a retired schema, must stay representable in code for as long as any older version is still supported.

## Alternatives considered

Available evidence does not preserve any alternative that was considered.

## Links

- [ADR-0011: Older stored formats are migrated outside current-format code](0011-older-stored-formats-are-migrated-outside-current-format-code.md), which this ADR amends
- [ADR-0012: Migrations produce current stored data, never the in-memory model](0012-migrations-produce-current-stored-data-never-the-in-memory-model.md)
