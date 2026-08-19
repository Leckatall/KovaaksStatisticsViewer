---
type: changed
area: Settings
user: Series configuration edits now use one unified store update operation.
---
`ISeriesConfigStore` now exposes `updateSeries` for both primitive and computed series, removing the retired split-type update APIs.
