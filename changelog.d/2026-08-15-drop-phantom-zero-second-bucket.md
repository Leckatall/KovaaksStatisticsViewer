---
type: fixed
area: Graphing
user: Run graphs no longer open on a spurious zero-value point at 0s, which was also dragging the y-axis down to 0 unnecessarily.
---
`PerfColumnBuilder::build` allocated one bucket per whole second from `t=0` through the run's last
rounded timestamp, but real `.perf` data never lands exactly on `t=0` — the first tick is always some
fraction of a second in. That left bucket 0 permanently zero-initialized across every column
(Score, Accuracy, ScoreTotal, etc.), producing a phantom leading point on every graph.
Bucket 0 is now dropped after bucketing (`buckets.erase(buckets.begin())`), and `result.times` is
reindexed from 1 instead of 0 to match. `PerfColumnBuilderTest` and `GraphUseCaseTest` cases that
happened to seed their first data point at exactly `t=0.0` were shifted by 1s (or index) to avoid
landing on the now-dropped bucket.
