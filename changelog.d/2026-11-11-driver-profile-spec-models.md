---
type: internal
area: Build & packaging
---
`driver::ScenarioSpec::modelled` - a profile-spec scenario entry can now reference a scenario-spec (`scenarioSpec`) and a performance-spec (`performanceSpec`) by paths relative to the referring spec; errors inside a referenced file are reported under the referring field's path.
`idealScore` rescales the scenario-spec's scoring so skill 1.0 lands on that score, letting one representative spec serve every scenario in its score band; `name`/`hash`/`durationSeconds` on the entry override the spec's (a new name without a hash makes a scenario of its own).
`skill` (constant) or `progression` (linear keyframes over runs or days, plus per-run `fluctuation` and per-day `dayFluctuation`) sets each run's skill. `handRuns` replaces a run at a schedule index with its own skill and/or performance-spec; fluctuation never applies to it, and without a skill it takes the progression trend at its position.
Seeds come from `gallery::mixSeed` over (scenario index, purpose, run or day), so fluctuation and shape noise draw from separate streams; legacy entries still go through `gallery::makeRun` with their old seeds and write the same profile as before.
