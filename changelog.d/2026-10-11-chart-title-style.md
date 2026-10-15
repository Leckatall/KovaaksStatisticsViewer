---
type: changed
area: User Interface
user: Chart titles on the Scenarios and Benchmarks tabs now share one style: left-aligned, semibold, one size.
---
New `ChartTitle.qml` (a `Label`: `Text.AlignLeft`, `ElideRight`, `Font.DemiBold`, 14px) is the single place the chart title style lives; revert or retune it there (a trial).
Used by `DashboardGraphCanvas` (`scenarioTitleLabel`, was centred bold 16px), `ScenarioHistoryPanel` (`historyTitleLabel`), `PlaytimeGraphPanel` (`playtimeTitleLabel`) and `BenchmarkHistoryCard` (`<prefix>LineName`, was left bold at the default ~12px). 14px sits between the two old sizes. Title texts, objectNames and layout margins are unchanged.
Tests: `tst_DashboardGraphCanvas::test_titleIsLeftAlignedOverThePlot` replaces `test_titleIsCentredOverThePlot`; `tst_BenchmarkWorkspace::test_nonEmptyHistoryCardsAreSeparateWithNamedAccessibleLines` checks the card titles' style.
