---
type: internal
area: Architecture
---
ADR 0011 records that code reading or writing persisted state knows only its current stored format, with older formats (including superseded settings keys) migrated by separate code reached from a single hand-off point. The run profile already conforms; benchmark definitions, graph configuration and superseded settings keys are recorded as debt. The architecture views have not been reconciled with it.
