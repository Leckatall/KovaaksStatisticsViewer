---
type: changed
area: Benchmarks
user: When a subcategory holds back your next tier, it is now marked once on the subcategory, with a hint naming its closest scenario, instead of on every scenario in it. The "blocked by" count now counts what you actually have left to clear.
---
`RankBlocker` now models one unmet rank requirement: `{optional<GroupId> group; vector<RankBlockerScenario> scenarios}`. A requirement is a subcategory, a category without subcategories, or a single Uncategorized scenario (no group).
`Benchmark::Builder::assignNextTier` pushes one blocker per unmet requirement. Previously it pushed one per non-attaining scenario of each unmet requirement, so a subcategory with three scenarios counted three blockers when any one of them reaching the tier would clear it.
`ProgressGridGroup` gains `blocker`, `blockerColor` and `blockerText`. `GridBuilder` marks the group cell and leaves the group's rows unmarked; only Uncategorized blocking rows keep a row marker. The group tooltip names the closest played scenario, chosen by highest `normalizedRank()` so different ladders compare, with the first in definition order winning a tie.
`BenchmarkTrackingViewModel::nextTierBlockerText` reads "(blocked by N group(s) and M scenario(s))", and `nextTierBlockerCount` counts requirements.
`ProgressGridGroupCell.qml` draws the blocker diamond after the rank chip. The table capture draws it the same way and shows the "Holding back the next tier" key entry when a group blocks.
`BenchmarkStatusSummary.qml`: the blocker suffix wraps inside its half of the card, so the longer text never pushes the right-hand fields across.
