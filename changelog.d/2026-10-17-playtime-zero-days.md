---
type: fixed
area: Graphing
user: Playtime graphs now show zero playtime on inactive days instead of joining activity across gaps.
---
`PlaytimeGraphUseCase` fills missing calendar days between measured playtime samples with zero, preserving rolling averages and range filtering without changing shared profile accounting.
