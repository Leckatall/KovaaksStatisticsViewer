---
type: added
area: Settings
user: Graph line changes in Settings now stay pending until you hit Save or Discard — the dashboard graph updates live as you edit, but nothing is written to disk until you confirm.
---
`SeriesConfigStore` gains `beginDraft()`/`commitDraft()`/`discardDraft()`/`hasPendingChanges()`.
`commitLocked()` now skips `writeLocked()` while a draft is active, still updating the in-memory `m_configs`/`m_next` cache — every existing consumer (`GraphViewModel`, `SeriesModel`, `DashboardGraphCanvas.qml`) keeps reading live state through `getAll()`/`onChanged()` unmodified.
`discardDraft()` restores the pre-draft snapshot and fires `onChanged` once; `hasPendingChanges()` compares current shadow state against the draft baseline via the existing `encode()` JSON round-trip (`SeriesConfig` has no `operator==` and its `Expression` is a `shared_ptr`, so structural comparison goes through the same encoding `commitLocked()`'s own no-op check already uses).
Passthroughs added to `ISeriesManagementUseCase`/`SeriesManagementUseCase` and `SettingsViewModel` (`pendingChanges` property, `beginSeriesDraft`/`commitSeriesDraft`/`discardSeriesDraft` invokables).
New `SeriesConfigDraftPanel.qml` wraps the existing enable/disable series list plus Save/Discard buttons, replacing the inline `Repeater` previously in `SettingsDialog.qml`'s Graph Lines page; calls `beginSeriesDraft()` on `Component.onCompleted`.
`SettingsDialog.qml` is now an `ApplicationWindow` (`flags: Qt.Dialog`) instead of a `Dialog`/`Popup` — `Popup`/`Dialog` has no cancelable close signal (`accept()`/`reject()`/`close()` always close unconditionally), whereas `Window`'s `closing(CloseEvent)` does, firing uniformly for every close path (title-bar X, Alt+F4, the footer's Close button). `onClosing` guards on `pendingChanges`, opening a `Save`/`Discard`/`Cancel` `MessageDialog` prompt instead of closing.
