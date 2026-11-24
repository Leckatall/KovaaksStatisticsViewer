---
type: changed
area: Benchmarks
user: Manage Benchmarks now lives in the File menu, and the separate Benchmarks menu is gone.
---
`AppMenuBar.qml` — `manageBenchmarksAction` moved from the removed `&Benchmarks` menu into `&File`, ahead of Settings. `tst_AppMenuBar.qml` now addresses Help as `menuAt(2)`.
