---
type: internal
area: Benchmarks
---
`evaluateBenchmark` split into named phases: `indexRuns`, `buildScenarioTree`, `assignGroupTiers`, `assignNextTier`, `sweepRankHistory`.
The rank rules no longer look entries up by id: `buildScenarioTree` records each group as an index range into the `EntryState` vector, which is index-parallel with `BenchmarkProjection::scenarios`, so `IndexById` and its per-entry string hash lookups are gone.
A category's tier rule is modelled as a list of `Requirement`s (one per subcategory, or its direct scenarios as one), which `TierRules::satisfied`, the subgroup tiers and the next-tier blockers all share instead of each re-branching on "subcategories, else scenarios".
`EntryState` gains `attains`, `thresholdAt` and `normalized`; `normalizedFromLadder` takes the optional ladder itself, removing the repeated `ladder ? ... : 0.0`.
`EntrySweep::pushRecent` / `offerBest` return each run's change to the running sums. Behaviour is unchanged.
