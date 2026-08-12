---
type: fixed
area: Data & profile cache
user: Run stats (shots, hits, misses, kills, damage, score) could be undercounted when a run had multiple events at the same timestamp — only the last event counted instead of the sum. Values now accumulate correctly.
---
`ScenarioPerf::add_data()` assigned each field (`point.shots = v`, etc.) instead of accumulating it,
so repeated calls for the same `(time, DataPointType)` pair — multiple events landing in the same
`ScenarioDataPoint` bucket — overwrote rather than summed. Now uses `+=` for all seven fields
(`shots`/`hits`/`misses`/`kills`/`dmg`/`dmg_possible`/`score`).

`ScenarioDataPoint` gained an explicit `ScenarioDataPoint(float time)` constructor zero-initializing
the rest, replacing the brace-init call sites in `get_data_point()` and
`ProfileSerializer` that only set `time`. `ScenarioCompletionData::scenario_time` renamed to
`scenario_length` to match `ScenarioPerf::scenario_length`, the field it's populated from.
