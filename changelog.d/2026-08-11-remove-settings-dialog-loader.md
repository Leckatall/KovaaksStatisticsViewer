---
type: fixed
area: Settings
user: The Settings dialog now opens centered on the window instead of pinned to the top-left corner.
---
`SettingsDialog` is now instantiated directly in `Main.qml` instead of behind a lazy `Loader`
(`settingsDialogLoader`). Under the `Loader`, the dialog's parent was the `Loader` item itself,
which has no explicit size or position and defaults to `0x0` at `(0, 0)` — so `anchors.centerIn:
parent` centered the dialog on that zero-size item rather than the window, pinning it to the
top-left corner. Direct instantiation parents it to the window's content item, so centering
resolves correctly. `AppMenuBar::settingsRequested` now just calls `settingsDialog.open()`.
Updated `DashboardUiTest.SettingsMenuActionOpensTheSettingsDialog` in
`tests/integration/dashboard_ui_test.cpp`, which previously asserted the dialog object didn't
exist until first opened — that assumption no longer holds since the dialog exists from startup.
