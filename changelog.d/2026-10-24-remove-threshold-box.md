---
type: changed
area: Benchmarks
user: Rank threshold scores moved to avoid the current score now appear without a box.
---
`TierBar.qml` removes both outline passes around threshold labels over partial fills, along with target-only state and outline padding in marker collision checks. Threshold positioning and text contrast over filled bars remain in place.
