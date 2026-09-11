---
type: changed
area: Graphing
---
Replaced the mixed numeric/calendar `AxisModel` factory with a read-only `Axis` rendering contract and dedicated persistent `ValueAxis` and `DateTimeAxis` implementations.
Graph view models now declare stable axis configuration at ownership sites and update ranges in place; benchmark history graphs share one parent-owned date axis.
Calendar axes accept typed `QDateTime` input, retain explicit timezone policy, and format labels according to the selected calendar interval.
