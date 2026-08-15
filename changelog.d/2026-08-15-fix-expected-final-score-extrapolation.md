---
type: fixed
area: Graphing
user: Expected Final Score and Expected Final Score (5s) now project accurately throughout a run instead of consistently landing under the real total, especially on scenarios that manipulate time flow.
---
`PerfColumnBuilder`'s `ExpectedFinalScore`/`ExpectedFinalScoreRecent` lambdas extrapolated pace-so-far
out to `ScenarioPerf::scenario_length`, the game-reported scenario duration. On scenarios that
manipulate time flow to control target speed, that field reflects scaled time rather than real elapsed
time, so it didn't match the number of one-second buckets the run's data actually spans - the
projection was systematically low (often 10-20% under ScoreTotal by the finish) instead of converging
to it.
Both lambdas now extrapolate against `ctx.buckets.size()` (the run's observed duration from its own
data) instead of `scenario_length`, which makes the projection exactly equal ScoreTotal at the final
bucket. The now-always-true `totalDuration > 0.0F` fallback guard was dropped since buckets is never
empty when these lambdas run.
