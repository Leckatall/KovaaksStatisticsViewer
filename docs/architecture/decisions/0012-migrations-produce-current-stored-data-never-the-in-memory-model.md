---
status: accepted
date: 2026-11-22
amends:
  - 0011
---

# 0012: Migrations produce current stored data, never the in-memory model

## Context

[ADR-0011](0011-older-stored-formats-are-migrated-outside-current-format-code.md) keeps older stored formats out of current-format code, but explicitly leaves open how a migration is carried out: it may upgrade older data to the current stored format, or decode it directly into the in-memory model.

The run profile takes the second route. `IProfileMigrator::migrate` returns `std::optional<domain::UserProfile>` ([`i_profile_migrator.h`](../../../src/data/interfaces/i_profile_migrator.h)), and its implementation for the retired version 3 builds the `UserProfile` itself, including enriching each run through `IRunIngestor` ([`profile_v3_migrator.cpp`](../../../src/data/formats/protobuf/migration/profile_v3_migrator.cpp)). ADR-0011 cites this migrator as the store that already conforms.

A migration that builds the in-memory model is an older copy of production code: it repeats how the model is constructed, and a change to that construction has to be made in both places. A migration that only converts older data to the current version and hands it to production code repeats nothing. The benchmark definition store, which is about to gain a new schema, will be the first store built this way.

## Decision

Migration code converts older stored data into the current stored format and hands the result to current-format code. Only the current-format reader builds the in-memory model; no migration code constructs any part of it.

This applies to every body of persisted state that ADR-0011 governs.

This decision does not govern how many steps a migration takes, whether or when migrated data is written back, which older formats are supported or for how long, or how unreadable data is rejected.

The run profile's migration, which decodes its retired version directly into the in-memory model, is nonconforming architectural debt. Recording this decision does not require it to be restructured now.

## Consequences

- The in-memory model is built, validated and enriched in one place, so a fix to the current-format reader applies to migrated data too.
- Migration tests compare stored data with stored data and need no in-memory model types.
- Migration code must be able to write the current stored format, so each store's current format has to be expressible from its migration code.
- Migrated data takes an extra encode and decode pass, and anything the current stored format cannot express is lost in migration.
- The run profile's migration needs restructuring before it complies. Until then, changes to it must not extend its direct construction of the in-memory model.
- ADR-0011 no longer leaves the choice between upgrading and decoding directly to each store.

## Alternatives considered

- **Decode older formats directly into the in-memory model**, as the run profile does now and as ADR-0011 permitted. Rejected: migration code becomes an older copy of production code, duplicating how the model is built.

## Links

- [ADR-0011: Older stored formats are migrated outside current-format code](0011-older-stored-formats-are-migrated-outside-current-format-code.md), which this ADR amends
- [ADR-0013: Migrations upgrade stored data one version at a time](0013-migrations-upgrade-stored-data-one-version-at-a-time.md), which governs the number of steps
