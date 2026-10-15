---
type: changed
area: Settings
user: Settings opens larger (about 860×620) so the Graph Lines list and folder paths have room, and it reopens at the size you last left it.
---
`SettingsDialog.qml` (a separate `ApplicationWindow`) now opens at 860×620 instead of 680×460. `open()` calls `_fitToScreen()` when the window is not already shown: it takes the remembered size or the default, raises it to the 680×460 floor (the old size), caps it at `_limitWidth()/_limitHeight()` (the larger of the main window, i.e. `transientParent`, and the screen's available area, so it only shrinks below the floor when both are too small), and centres it over the main window, kept at or after the screen's top-left. `minimumWidth/minimumHeight` follow the same floor-or-limit rule.
The main window already remembers its size through `VisualSettingsManager`'s QML `Settings { category: "window" }`, so the dialog does the same: a `Settings { category: "window" }` with `settingsDialogWidth`/`settingsDialogHeight`, written when the window hides.
"Fits inside the main window or screen" uses the larger of the two because the driver's offscreen screen is 800×800, smaller than its 1280×800 main window; a real desktop's screen is at least the main window's size, so there the screen bounds it.
Test: `tst_SettingsDialog::test_opensLargeAndRemembersItsSize`. Driver: 860×620 at 1280×800 and at 960×700 (x 52, y 42, inside the 960×700 main window).
