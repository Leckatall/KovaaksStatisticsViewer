---
type: fixed
area: User Interface
user: View menu labels now line up: submenus and plain items no longer sit further left than the items with check boxes.
---
Fusion's `MenuItem` indents a checkable item's label past its check box (the content item's `leftPadding` is the indicator width plus spacing) but gives submenu items and plain actions no indent, so in the View menu "Scenario Graph Lines" started 20px left of "Scenario Graph" (x 37 vs 57 in the driver), and in the Scenario Graph Lines submenu "Configure Lines..." sat left of the series check items.
`AppMenuBar.qml` gains an inline `AlignedMenuItem` component whose `leftPadding` adds the indicator width plus spacing when the item is not checkable; the View menu and the Scenario Graph Lines submenu use it as their `delegate` (the delegate builds the items for Actions and submenus; the explicitly declared checkable `MenuItem`s are unchanged). Every label in those menus now starts at one x (57 in the View menu, 255 in the submenu at 1280x800).
Test: `tst_AppMenuBar::test_viewMenuLabelsStartAtTheSameX`.
