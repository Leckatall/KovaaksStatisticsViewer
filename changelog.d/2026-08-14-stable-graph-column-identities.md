---
type: internal
area: Architecture
---
Dashboard graph column ids now have explicit values and application-owned stable string keys. `GraphViewModel::columnKey()` delegates to that contract, allowing persistent graph preferences and future app-layer selection logic to identify columns without depending on presentation metadata.
