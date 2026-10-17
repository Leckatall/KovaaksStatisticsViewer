---
type: changed
area: Benchmarks
user: The benchmark name and its summary now stay pinned at the top while you scroll. Pick a benchmark from a searchable dropdown under its name, listed most recently viewed first. Manage Benchmarks moved into that dropdown, and Next tier now shows how many scenarios block it.
---
`BenchmarkPicker.qml` — new signal-out component (`benchmarkSelected`, `manageBenchmarksRequested`, `editBenchmarkRequested`) replacing the always-visible benchmark list in `BenchmarkWorkspace.qml`.
The title is a bold name with a chevron, the popup has a case-insensitive search field, the entries in `selectorEntries` order and a "Manage Benchmarks…" row outside the filtered list.
Incomplete, invalid and unsupported entries keep their chip and edit button; the edit button closes the popup.
`benchmarkHeader` is now a pinned card outside the scrolling body, holding the picker, `benchmarkClassificationLabel` and, in the trackable state only, `BenchmarkStatusSummary` (instantiated by a `Loader`, no longer in `BenchmarkTrackableBody.qml`).
The header Manage Benchmarks button, `_bodyOwnsManagerAction` and the `nextTierProgress` label (with its "Not ranked yet:" prefix) are removed; the progress grid is now the first item of the scrolling body.
`BenchmarkStatusSummary` drops its own frame and shows `<tier> (blocked by N scenario[s])` through `summaryNextTierBlockers`, sourced from `nextTierBlockerCount`.
`IRecentBenchmarks` / `RecentBenchmarks` (`src/app/usecases/`) — new app-layer entity over `ISettingsService`, built once in `App::App()` and injected into `BenchmarkTrackingUseCase` (replacing its `ISettingsService`) and `BenchmarkManagerUseCase`.
`select()` marks a benchmark viewed (moves it to the front); a successful create in the Manager marks it created (inserted right behind the front); `selectDefault()` takes the first remembered id that is still selectable. Ids are never pruned or capped.
`ISettingsService` stores the list under `benchmarks/recentIds`; `getRecentBenchmarkIds()` falls back to a one-item list from the legacy `benchmarks/lastSelectedId` while the new key is absent, and the first write removes the legacy key. `get/setLastSelectedBenchmarkId` are gone.
`BenchmarkChoice::recency` carries each id's index in the list; `buildSelectorEntries` orders loaded entries by recency, then by name, with problem entries last by filename.
