---
type: added
area: Graphing
user: New scenario history graph, plotting score, accuracy, shots, hits and misses across every run of the scenario you're currently viewing.
---
`CompletionHistoryViewModel` adds an integral run-index graph with independent metric axes and persistent line visibility controls.
`CompletionHistoryUseCase` follows the current scenario and refreshes when its history changes, while `UserProfile::getCompletionHistory()` projects chronological completion totals without copying full run data.
`GraphCanvasWithTooltip` now supports graph-specific X-axis labels.
