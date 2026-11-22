---
type: internal
area: Architecture
---
ADRs 0012 and 0013 amend ADR 0011 to settle how a migration is carried out, which 0011 had left open.
0012: migration converts older stored data into the current stored format and hands it to current-format code; only the current-format reader builds the in-memory model. The run profile's `IProfileMigrator`, which returns a `domain::UserProfile` built by `ProfileV3Migrator`, is recorded as nonconforming debt.
0013: older data is upgraded one version at a time (vN → vN+1 → … → current), each step knowing only its source and target formats; a failed step is reported as `Migration from version x to version x+1 failed: <reason>`.
ADR 0011 gains an Amendments section pointing at both. No code changed; benchmark definitions are the intended first store built this way.
