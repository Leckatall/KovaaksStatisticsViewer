---
type: fixed
area: Data & Profile
---
`ScenarioPerf::add_data()` now validates the value's type against the `DataPointType` category before calling `get_data_point()`, so a rejected call (e.g. a float passed for `SHOTS`) no longer inserts a zero-valued phantom `ScenarioDataPoint` at that timestamp. Previously `get_data_point()` ran first and unconditionally appended, so the phantom point survived the subsequent throw.
