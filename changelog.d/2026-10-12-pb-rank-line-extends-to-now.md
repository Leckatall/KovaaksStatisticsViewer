---
type: changed
area: Graphing
user: The Benchmarks "Personal-best average rank" line now continues flat to the right-hand edge of the graph, so your current rank can be read there instead of the line stopping at the last date it changed.
---
`BenchmarkTrackingViewModel::rebuild()` appends one point to the rank series at `m_historyXAxis.max()` (the shared axis' right edge, which is rounded up to a tick and may also be set by the playtime series) carrying the last rank value, when the last rank point lies before that edge. The series stays a spline over those points, so the tail is flat. With no rank history nothing is added and the empty state is unchanged. The playtime series is untouched.
Tests in `benchmark_history_vm_test.cpp`: the new `RankLineIsHeldFlatToTheRightEdgeOfTheAxis`; the calendar-day conversion test now accepts the extra trailing point.
