---
type: changed
area: Data & profile cache
user: Generating a profile no longer freezes the app — the window stays responsive while your runs are scanned, and the results appear when it finishes.
---
Profile generation moved off the UI thread. `ProfileBuildWorker` (`src/app/profile_build_worker.h`) is a `QObject` living on a `QThread` owned by `SessionController`; its `build()` slot runs `data::ProfileBuilder` and emits `finished(UserProfile)`, delivered back to the UI thread as a queued connection. `Q_DECLARE_METATYPE(ksv::domain::UserProfile)` sits in the worker header rather than beside the type, keeping `domain` free of Qt.

All three build paths are async, via one mechanism: `IProfileService::onBuildRequested()` takes a callback that `SessionController` installs, and `loadProfile()`'s cache-miss branch calls it instead of building inline. With no requester installed the old synchronous `generateProfileFromDirectory()` still runs, which is why the data-layer tests are unaffected. `App::App()` now constructs `SessionController` *before* the first `loadProfile()` — otherwise startup's cache miss would build synchronously and the reordering is the whole point.

Concurrency handling:

- **New runs mid-build.** Between `beginProfileBuild()` and `applyBuiltProfile()`, `ProfileService` queues watcher-supplied perf paths instead of mutating the profile. On completion it swaps the built profile in, then replays the queue, skipping runs the build already picked up (a `getRun()` check ahead of `addScenarioPerf`, which would otherwise log a duplicate). A file landing before the scan is in the result; one landing after is replayed; neither is lost or applied to a profile that is about to be discarded.
- **Stale results.** A build whose `UserProfile::getSourceDirectory()` no longer matches the file service is dropped — the Kovaaks dir can be repointed mid-build. The pending queue deliberately survives the discard so the follow-up build still replays it. `SessionController` coalesces a generate requested while one is in flight into a single rebuild rather than dropping it.
- **`SettingsService` is now mutex-guarded.** The worker reaches `QSettings` through `IFileService::getSourceDirectory()`/`getAllPerfsFromFiles()` → `getKovaaksDir()` while the UI thread reads and writes the same instance; `QSettings` is reentrant but not thread-safe when shared. The lock covers only `m_settings` access — the change callbacks fire outside it, since they re-enter the getters. `ProtoDecoder::decode_file` needed nothing: it is stateless.

`SessionViewModel::generateProfile()` can no longer refresh the scenario list inline, so `ISessionController` gained a `profileChanged()` signal, re-emitted from the existing `onProfileChanged` subscription. `SessionViewModel` refreshes its hash map off it, and `ScenarioBrowserViewModel` refreshes its cached summaries — the latter previously relied on `currentPerfChanged`, which is suppressed when the newest run is unchanged.

No progress UI; the button is not disabled during a build.
