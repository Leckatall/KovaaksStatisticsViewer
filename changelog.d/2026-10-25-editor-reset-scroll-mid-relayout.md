---
type: fixed
area: Benchmarks
user: Opening a benchmark with wider ranks while scrolled through the editor no longer lands part-way into a rank.
---
`BenchmarkEditorTable.relayoutRankColumns` now reads the pending reset scroll before assigning `laidOutThresholdWidth`, and `restoreRankScrollAfterReset` is suppressed during that assignment. The assignment moves the views' left margin, which can lay the threshold view out synchronously and run the restore while `rankScrollStart` still reflects the old width — consuming the pending scroll and landing mid-rank.
