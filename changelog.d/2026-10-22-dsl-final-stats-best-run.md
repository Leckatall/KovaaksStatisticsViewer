---
type: added
area: Graphing
user: Series expressions can now use a run's final stats (`Final(SCORE)`) and your best run (`BestRun(...)`), e.g. to draw your high score or PB pace.
---
`FinalValue` (`Final(<STAT>)`, stats `SCORE|SHOTS|HITS|MISSES|KILLS|DMG` read from `Run::stored_totals`) and `BestRun` (unary; evaluates its input on the scenario's highest-scoring other run, earliest start on ties, no length requirement) are new `ExpressionNode` alternatives, with DSL encode/decode, `InvalidFinalStat` validation, `EditableFinalValueNode` (registered in `declare_metatypes()`), `bestRun` on `EditableUnaryOpNode`, and QML editor support.
`Final` takes a bare stat token, the one exception to the labelled-parameter rule in `expression_dsl.h`.
`AverageLineUseCase` now threads a `graphLength` (the current run's bucket count) through evaluation: `Final(...)` and `NumericConstant` produce vectors of that length rather than the evaluation run's, so `BestRun(Final(SCORE))` works for CSV-only best runs. A purely constant `AverageAcrossRuns` input therefore admits candidates of any length.
`domain::RunTotals` gains `std::optional<float> damage` (CSV `Damage Done:`, summed perf `dmg`), persisted as `RunTotals.damage = 6` in `profile.proto` without a store-version bump; the profile importer ignores damage when only one side has it.
Refresh Profile skips already-known source files, so runs imported before this change keep `damage == nullopt` (and `Final(DMG)` draws nothing for them) until their source files change.
