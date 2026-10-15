---
type: changed
area: Graphing
user: The Scenarios tab now shows Scenario History right under the run graph, with the all-scenario Daily Playtime graph last; the View menu lists the graphs in the same order.
---
`ScenarioWorkspace.qml`: the `graphPanelColumn` ColumnLayout now holds `scenarioGraphPanel` → `scenarioHistoryPanel` → `playtimeGraphPanel`, so the two panels about the selected scenario sit together. Equal-height sharing (`Layout.preferredHeight: 1` on each) and the visibility bindings are unchanged.
`AppMenuBar.qml`: the "Playtime Graph" toggle moved after the "Scenario History Lines" submenu, matching the panel order.
Tests: `tst_AppMenuBar::test_viewMenuGraphEntriesFollowPanelOrder` and `DashboardUiTest.GraphPanelsAreOrderedRunHistoryPlaytime`.
