---
type: changed
area: User Interface
user: "Set Source Directory..." and "Settings..." in the File menu now end in "..." like the other menu items that open a dialog.
---
`AppMenuBar.qml`: `Set Source &Directory` → `Set Source &Directory...`, `&Settings` → `&Settings...`, matching `&Load Performance File...`, `&Manage Benchmarks...` and `Configure Lines...`. Mnemonics unchanged.
`&About` is left without an ellipsis: per Windows convention the ellipsis marks a command that needs more input before it acts, and About only shows information.
No tour selector used the old texts (`tools/driver/tours/*.txt` reach Settings through `settingsDialog.open()`); the hint in `ScenarioSearchPanel.qml` names the menu path "File › Set Source Directory" without the ellipsis, as menu paths are written.
Tests: `tst_AppMenuBar::test_dialogItemsEndInEllipsis`; the Set Source Directory and Settings trigger tests look items up by their new text.
