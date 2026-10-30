---
type: changed
area: Graphing
user: The default run graph now has a High Score line, draws lines based on your other runs at 50% opacity, and uses shorter Exp. Score names that show which ones are projected from the last 5 seconds. Existing setups keep their lines until you restore the default graph lines.
---
`defaultSeriesConfigs()` gains `SeriesId{9}` "High Score" (`BestRun(Final(SCORE))`, Score axis). `BestRun` skips the viewed run, so the line is the best other run and is absent when there is none.
Series 3, 4 and 9 use alpha 128; series 8 stays opaque because it is computed from the current run.
Series 2, 3, 4 and 8 are renamed; ids and expressions are unchanged, so stored documents need no schema bump or migration.
