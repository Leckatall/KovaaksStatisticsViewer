---
type: changed
area: User Interface
user: The menu bar now reads File, View, Benchmarks, Help, with Help last as in other Windows apps.
---
`AppMenuBar.qml`: the `&Help` menu moved after `&Benchmarks`. Mnemonics are unchanged (Alt+B and Alt+H still open their menus; checked with the driver).
Tests: `tst_AppMenuBar::test_menusFollowWindowsOrder`; `test_aboutRequested_emittedWhenActionTriggered` now reads `menuAt(3)`.
