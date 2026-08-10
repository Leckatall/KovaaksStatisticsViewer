---
type: fixed
area: Data & profile cache
user: Changing your KovaaKs folder in Settings now takes effect immediately — previously new runs stopped being picked up until you restarted the app.
---
`ISettingsService` gained `onKovaaksDirChanged`, mirroring the existing `onProfilePathChanged`, and
`SettingsService::setKovaaksDir` now fires it. `FileService` subscribes and repoints its
`QFileSystemWatcher` to the new `FPSAimTrainer/performances` directory, re-snapshotting `m_known_files`
so pre-existing files there aren't reported as new.
