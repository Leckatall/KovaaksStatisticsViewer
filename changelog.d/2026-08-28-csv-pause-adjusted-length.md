---
type: fixed
area: Data & Profile
user: CSV-only runs that were paused now report the actual scenario length instead of counting paused time.
---
`RunIngestor::buildRun` subtracts the CSV footer's `Pause Duration` from the wall-clock length derived for perf-less runs, clamped at zero; `start_time` stays anchored to the real start, and a paired `.perf`'s decoded length still takes precedence.
