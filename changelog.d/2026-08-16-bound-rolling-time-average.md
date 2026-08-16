---
type: fixed
area: Graphing
---
`UserProfile::rollingTimeAverageFor()` no longer allocates a dense per-calendar-day vector spanning the full first-to-last-run range. It now walks forward only while a trailing window-sum stays non-zero, jumping directly to the next recorded play day once it hits zero, bounding total output to `daily_totals.size() * window_days` instead of the calendar gap between the first and last run (which could be years for a returning player). Produces identical output to the previous algorithm for every existing case; only a run separated from its neighbor by more than `window_days` days stops materializing the dead days in between.
