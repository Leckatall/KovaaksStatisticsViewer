---
type: added
area: Data & Profile
user: Older CSV-only KovaaK's runs now appear in run history, while current runs retain CSV settings and authoritative totals.
---
Replaced `ScenarioPerf` with a facet-based `Run` model, storing totals once and retaining optional protobuf performance samples and CSV footer stats.
`RunIngestor` owns stem pairing and is shared by full scans, live perf arrivals, and v3 profile migration; graph analysis empty-states runs without performance samples.
Renamed the current store schema to `profile.proto` at version 4 and isolated the retired v3 descriptor/upgrade path in a droppable migrator.
