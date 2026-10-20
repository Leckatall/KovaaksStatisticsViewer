---
type: internal
area: Build & packaging
---
UI driver snapshot fixtures: `tools/driver/snapshots/2026-10-17/` holds the real app's `profile.pb` and benchmarks as an overlay plus a `start.txt` batch replaying its registry settings as `--setting` (minus `file/…` and the dead `graph/seriesConfigs`). `tools/driver/snapshots/take_snapshot.py` takes a new one, decoding Qt's registry `@Variant(...)` values (which carry no QVariant null flag) into JSON.
