---
type: internal
area: Scenario & run selection
---
`domain::RunData` replaces `ScenarioCompletionData` and domain `RunPerformance`, folding the run identity and aggregate statistics into a single domain projection.
`application::RunPerformance` owns the browser-specific derived `personal_best` flag; `ScenarioBrowserUseCase` calculates it from the complete chronological scenario history and `RunListModel` exposes the unconsumed `personalBest` role for a later UI change.
