---
type: changed
area: Graphing
user: Playtime dates now use calendar-aligned graph bounds and tick marks instead of arbitrary epoch-day intervals.
---
`AxisModel` now has a separate UTC epoch-millisecond date-time range API, keeping numeric `Baseline` handling unavailable to calendar axes. `PlaytimeGraphViewModel` converts its discrete epoch-day input at the presentation boundary and uses the new calendar-aware ticks.
