---
type: added
area: User Interface
user: Keyboard shortcuts: Ctrl+, opens Settings, Ctrl+O loads a performance file, and Ctrl+F jumps to the scenario search from either tab. The File menu shows the first two.
---
`AppMenuBar.qml`: `shortcut: "Ctrl+O"` on the Load Performance File action and `shortcut: "Ctrl+,"` on the Settings action; Fusion's `MenuItemIconLabel` draws the shortcut text (`shortcutLabel`) at the item's right edge.
`Main.qml`: a window-context `Shortcut` (`findScenarioShortcut`, Ctrl+F) switches `workspaceTabBar` to Scenarios and calls `ScenarioWorkspace.focusScenarioSearch()`, which re-shows a hidden selection panel / scenario browser section (writing those view settings), then (next event loop turn, after the layout shows them) `SelectionPanel.focusScenarioSearch()` → `ScenarioSearchPanel.focusSearch()`: `forceActiveFocus(Qt.ShortcutFocusReason)` and `selectAll()`. On the first-run welcome it does nothing.
Conflicts checked: no other `Shortcut`/`shortcut:` used Ctrl+O, Ctrl+, or Ctrl+F (the only existing one is the editor table's application-wide Escape). All three are window shortcuts of the main window; Settings and the Benchmark Manager are separate windows, so typing in their text fields never triggers them.
Tests: `tst_AppMenuBar::test_fileMenuActionsCarryTheirShortcuts`, `tst_Main::test_ctrlFFocusesTheScenarioSearchFromEitherTab`. Ctrl+, opening `settingsDialog` is checked in the driver capture (`key Ctrl+,` → `settingsDialog.visible`), since the QML test harness has no Settings window double.
