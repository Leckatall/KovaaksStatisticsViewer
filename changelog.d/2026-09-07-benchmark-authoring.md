---
type: added
area: Benchmarks
user: Create, import from a Kovaak's playlist, edit, organize, validate, save, rename, delete, and reopen benchmark definitions — incomplete drafts are preserved, and every draft-replacing action confirms before discarding unsaved work.
---
`BenchmarkLibraryService` gains the single-draft lifecycle and
mutation commands (tiers, thresholds, scenarios, categories/subcategories with atomic reorganization)
plus playlist import and content-precondition-guarded save/rename/delete over `IBenchmarkRepository`.
New `IPlaylistReader`/`PlaylistReader` decode playlist exports with a duplicate report. New
`BenchmarkManagerViewModel` with a rebuilt display-tree of `BenchmarkTreeNode`s and a window-modal
`BenchmarkManagerDialog` following the `SettingsDialog` convention. The profile-known scenario picker
is deferred to slice 3; reconciliation and tracking output remain deferred.
