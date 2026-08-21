---
type: fixed
area: Settings
user: Reopening Settings after saving graph line changes now lets you edit and save again — previously every further edit persisted immediately and the Save/Discard buttons stayed disabled.
---
`beginSeriesDraft()` was called exactly once per app lifetime, from `SeriesConfigDraftPanel.qml`'s
`Component.onCompleted`. The panel is a plain, eagerly-instantiated child of `SettingsDialog`'s
`StackLayout` — not behind a `Loader` — and `SettingsDialog` is a long-lived `ApplicationWindow`
shown/hidden via `open()`/`close()`, never destroyed and recreated, so `onCompleted` never fired
again after the dialog's first construction.
Once `SeriesConfigStore::commitDraft()`/`discardDraft()` flipped `m_draftActive` back to `false`
(series_config_store.cpp), nothing ever set it back to `true`. `commitLocked` writes straight
through to `QSettings` immediately whenever `!m_draftActive`, so every mutation after the first
save/discard persisted instantly instead of batching into a new draft. `hasPendingChanges()` also
short-circuits to `false` whenever `!m_draftActive` regardless of what was just edited, which
permanently disabled the Save/Discard buttons in `SeriesConfigDraftPanel.qml` after the first
resolution.
Moved draft ownership to `SettingsDialog::open()`, which now calls `root.settingsVm.beginSeriesDraft()`
on every open, including reopens — removed the one-shot call from the panel entirely.
