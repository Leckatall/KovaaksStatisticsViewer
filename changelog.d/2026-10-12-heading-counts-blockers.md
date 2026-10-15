---
type: changed
area: Benchmarks
user: The line above the benchmark progress grid now says how many scenarios block the next tier, for example "1 scenario blocks Silver", matching the marked rows below it.
---
`BenchmarkTrackingViewModel` gains `nextTierBlockerCount` (size of `projection.nextTierBlockers`, the same list the progress grid marks rows from) and `hasNextTier`. `scenariosAtNextTier` stays on the view model and projection but the heading no longer uses it, because ranks are attained per group and that count disagreed with the blocker markers.
`BenchmarkWorkspace.qml` heading (`nextTierProgress`): "1 scenario blocks <Tier>" / "N scenarios block <Tier>" with the tier name in its colour; "Highest tier attained" when there is no next tier; "Not ranked yet: ..." prefix while the attained rank is Unranked; hidden when the benchmark is not trackable.
Tests: `BenchmarkTrackingViewModel` blocker count equals the grid's marked rows; QML tests for plural, singular, no-next-tier and unranked wording.
