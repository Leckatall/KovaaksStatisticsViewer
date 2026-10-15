---
type: internal
area: Benchmarks
---
The not-trackable case of the progress heading (`nextTierProgress` in `BenchmarkWorkspace.qml`) is now actually pinned.
The old `test_headingIsHiddenWhenNotTrackable` in `tests/ui/qml/tst_BenchmarkWorkspace.qml` passed whatever its fixture was: it fell back to an empty fixture if `trackingFixtureIncomplete` was missing, and accepted a visible heading as long as its text was empty, so it would also pass for a trackable fixture whose heading happened to be blank.
It is now data-driven over the incomplete and unavailable fixtures, each overlaid with stale next-tier values (`hasNextTier: true`, `nextTierBlockerCount: 3`, a coloured `nextTier`); it asserts the fixture is not trackable and has no summary, that the state's own body (`incompleteBody` / `unavailableBody`) is shown with no `benchmarkStatusSummary`, and that the heading is absent or hidden. Inverting the heading assertion, or making the fixture trackable, fails it.
New `BenchmarkTrackingViewModel.NonTrackableBenchmarkHasNoNextTierAndNoBlockers` (`tests/ui/benchmark_tracking_vm_test.cpp`) turns a trackable snapshot incomplete while keeping its projection's `nextTier` and `nextTierBlockers`, and checks that `hasNextTier` drops to false and `nextTierBlockerCount` to 0.
Checked once in the driver with an incomplete benchmark (no tiers, no scenarios) selected: the incomplete body shows its message, issues, Edit benchmark, total playtime and the playtime history card; there is no summary and no heading, and nothing looks broken.
