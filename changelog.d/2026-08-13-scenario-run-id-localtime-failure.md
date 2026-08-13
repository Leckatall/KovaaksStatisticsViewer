---
type: fixed
area: Domain
user: Scenario run labels remain safe when the operating system cannot convert their timestamp to local time.
---
`ScenarioRunId::toString` now checks `localtime_s`/`localtime_r` before formatting and falls back to the scenario name when conversion fails.
