---
type: internal
area: Settings
---
`SeriesConfig` rows no longer split into a protected "base" category and an editable "computed" one — `validateSeriesConfigs` dropped the exactly-one-row-per-`PrimitiveMetric`/canonical-name checks, and `SeriesConfigStore::updateSeries`/`removeComputed` dropped the `isPrimitive()` guard that blocked renaming, re-expressing, or deleting a primitive row. `validatePresentation`'s name checks (`EmptyComputedName`/`ComputedNameNotTrimmed`/`ComputedNameTooLong`) now run unconditionally instead of skipping primitive rows.
