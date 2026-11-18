---
type: changed
area: Settings
user: The Settings page now notices when your KovaaK's folder becomes valid, for example after your first run there, without you choosing the folder again.
---
`IProfileLocationsUseCase` / `ProfileLocationsUseCase` — new app-layer use case for the Profile page: the KovaaK's directory, its validity, the profile path and whether the profile is loaded. Wired in `App::App()` over `ISettingsService`, `IFileService` and `IProfileService`.
`SettingsViewModel` now depends only on `IProfileLocationsUseCase` and `IGraphConfigurationUseCase`, retiring the last ADR 0004 violation (view models reaching data-layer services directly). It no longer caches the directory; `kovaaksDirChanged`, `profilePathChanged` and `profileLoadedChanged` are emitted from the use case's notifications, so changes made by any writer reach the page.
The "replace the first source root, keep the rest" rule moved from the view model into the use case.
Old bug: `kovaaksDirValid` was re-read only when the directory was set through the view model, so a folder that gained its `FPSAimTrainer/performances` or `stats` directory later stayed invalid (Refresh Profile disabled, invalid-directory banner shown) until restart. The use case re-checks validity on `IFileService::onSourcesChanged` and publishes when it flips. That signal only fires when the watched listing changes, so an empty performances folder appearing does not flip it until a file lands there.
Removed a leftover `data/interfaces/i_benchmarks_service.h` include from `benchmark_manager_vm.h`.
