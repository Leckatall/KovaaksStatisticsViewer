---
type: added
area: Benchmarks
---
`evaluateBenchmark` — new pure domain function turning a definition, its completeness, per-entry resolutions and chronological run facts into an immutable `BenchmarkProjection`: per-scenario personal best, latest-five average, attained tier and next threshold; official attained and completed rank with next-tier blockers and partial counts; equal-weight average rank; and the personal-best-event daily average-rank history, which is reconstructed in full under the current definition rather than patched.
`thresholdLadder` gates every tier output per entry, so one scenario with a missing or non-increasing threshold loses its own rank without blanking the rest.
`BenchmarkLibraryService` gains `IProfileService` and becomes the single authority joining name-based definition membership to hash-based run identity: an exact-name catalogue, the five `ScenarioMatchState`s, automatic persistence of a unique mapping through the existing content-digest precondition, deferral while a draft is dirty, and re-reconciliation after save or discard with the open draft adopting the resulting digest. `setScenarioHash` is the explicit ambiguity choice, and it can clear a mapping so a `MappedUnavailable` entry can recover without losing its thresholds.
New `BenchmarkTrackingUseCase` on `IBenchmarkTrackingUseCase` owns the selected `BenchmarkId` and caches projections by benchmark id, library revision and profile revision; a command never returns a projection, so evaluation can move to a worker without changing any caller.
`UserProfile` gains `getRunFacts(scenarios)` and a `getRollingTimeAverage(scenarios, window)` overload delegating to the existing `rollingTimeAverageFor`, so benchmark playtime and the profile-wide playtime graph cannot drift apart.
`BenchmarkManagerViewModel` exposes match state, ambiguity candidates and the profile-known scenario catalogue; the picker UI and the tracking workspace remain deferred.
Wired in `App::App()`, which now builds the benchmark graph before the first `loadProfile()` so the startup profile is reconciled rather than published to nobody.
