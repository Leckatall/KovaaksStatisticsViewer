---
type: fixed
area: Data & profile cache
user: Fixed a rare startup crash if a new run's .perf file appeared before the profile finished loading.
---
`ProfileService::addPerfFileToProfile` now guards `m_profile` for null, matching the pattern already
used by `saveProfile()` and every read accessor. Previously it dereferenced `m_profile` unconditionally,
which could crash if `IFileService::onFilesChanged`'s callback fired (via `QFileSystemWatcher`) between
the subscription being registered in the constructor and `loadProfile()` running in `App::App()`.
