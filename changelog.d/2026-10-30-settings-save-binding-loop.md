---
type: fixed
area: Settings
---
Settings dialog's Save button no longer logs a "Binding loop detected for property enabled" warning when it is disabled while focused. `SettingsDialog.hasUncommittedEdit()` now reads a `focusedField` property instead of `activeFocusItem` directly.
