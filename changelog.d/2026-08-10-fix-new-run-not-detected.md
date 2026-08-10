---
type: fixed
area: Data & profile cache
user: New runs completed while the app is open are now correctly picked up and added to your stats — previously they were silently ignored until the app restarted.
---
`FileService` was forwarding the watched directory's own path (from
`QFileSystemWatcher::directoryChanged`) to `onFilesChanged` subscribers instead of the path of the
newly-created file, so `ProfileService::addPerfFileToProfile` tried to decode the directory itself and
failed silently. `FileService` now diffs `QDir::entryList` snapshots to report the actual new file(s).
