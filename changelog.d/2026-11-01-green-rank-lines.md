---
type: changed
area: Benchmarks
user: The personal-best and recent average rank graphs now draw their lines in two shades of green by default instead of white.
---
`kPersonalBestRankColor` and `kRecentRankColor` in `graph_configuration.cpp` are now the default colours of the `rank` series on `GraphKey::BenchmarkRank` and `GraphKey::BenchmarkRecentRank`, replacing `std::nullopt`, which left `GraphCanvas` drawing the foreground colour.
Saved series colours still win over the default; series without one pick up the new colour.
