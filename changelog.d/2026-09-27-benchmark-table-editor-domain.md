---
type: changed
area: Benchmarks
user: Benchmarks with an unnamed tier, scenario, category or subcategory are now flagged as incomplete, and threshold problems point at the exact tier cell.
---
`validateBenchmark` reports missing tier, scenario and group names per element, excludes blank names from duplicate-name checks, and locates threshold issues by tier ID. It reports each non-increasing threshold at its own cell.
`BenchmarkEditor` adds unnamed-row expansion, atomic ordered assignment, subcategory and scenario reordering, and explicit relocation of direct scenarios when adding the first subcategory. It rejects an unknown tier when clearing a threshold and shares the reordering implementation.
