---
type: fixed
area: Data & Profile
---
`UserProfile::getAverageScore()` now accumulates the running score total in `double` instead of `float`, casting to `float` only once at the final division. Previously, once the running total reached float32's `[2^26, 2^27)` range (ULP 8), individual run scores smaller than the current ULP were silently absorbed and dropped from the average.
