---
type: changed
area: Benchmarks
user: The average rank graph now only shows tiers up to one above your best rank, so your progress line is no longer squashed by tiers you have not reached.
---
`BenchmarkHistoryViewModel::update` caps the rank Y axis and tier reference lines at `floor(peak) + 1` (bounded by the tier count), where peak is the highest plotted rank.
