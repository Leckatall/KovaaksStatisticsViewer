---
type: added
area: Benchmarks
user: Each category and subcategory in the Benchmarks grid now shows its rank as a small chip in that tier's colour, so you can see which groups are holding you back.
---
`BenchmarkProgressGridModel` groups gain `rankText`, `rankColor` (readable text colour) and `rankBarColor` (opaque tier colour) from `GroupProjection::satisfiedTier`, the rank the old breakdown header showed as "highest Silver". A group without a satisfied tier is "Unranked" with no colour (a neutral chip); Uncategorized (never ranked as a group) and every group of a benchmark that is not Trackable get no chip (empty `rankText`). Lookup is `GridBuilder::addGroupRank`/`findGroup` over the projection's category tree.
`BenchmarkProgressGrid.qml`: the group cell's name and chip sit in one row; the chip is a pill with a 28% tint and a border in the tier colour and the rank in the readable tier colour (11px), neutral `palette.mid` border and placeholder text when unranked. Group column widths now include the chip (`rankChipSpace`), so the name never elides behind it; at 960x700 the varied demo's grid therefore scrolls horizontally inside its own frame.
Tests: `BenchmarkProgressGridModel.GroupsCarryTheirRankInTheTierColour` (ui-cpp), `test_groupCellsShowTheirRankChip` (tst_BenchmarkWorkspace.qml).
