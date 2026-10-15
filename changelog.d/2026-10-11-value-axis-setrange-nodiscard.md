---
type: internal
area: Graphing
---
`ValueAxis::setRange` is no longer `[[nodiscard]]`. Its bool only reports "range changed" and most callers (`series_model.h`, `BenchmarkHistoryViewModel`, many tests) ignore it, which produced repeated discarded-return warnings on every build. Callers that react to the change (`GraphViewModel`) still use the result. `DateTimeAxis::setRange` and `ValueAxis::setOptions` keep `[[nodiscard]]`.
