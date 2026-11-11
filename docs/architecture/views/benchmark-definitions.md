---
id: benchmark-definitions
name: Benchmark definitions
viewpoints: [information]
---

# Benchmark definitions

## Concern and scope

What identifies a benchmark or one of its elements? Which copy is authoritative, and how does the application load, upgrade or classify its files? This matters to anyone changing an ID, the JSON shape, a write path, or the rules that decide whether a definition can be tracked.

The view covers a definition from its stored file through the accepted in-memory snapshot to the derived scenario resolution, and the recent-benchmark IDs kept in settings. This view expands the benchmark data described in [Benchmarks][benchmarks]. [Benchmark tracking][benchmark-tracking] explains change publication; [Benchmark editing][benchmark-editing] explains working-copy edits and saves. Run and scenario identity belong to [Run history data][run-history-data]; the stored form of graph settings (including the benchmark graphs) belongs to [Graph configuration][graph-configuration].

## Architectural account

### Identity

| Entity | Identity or lookup key | Consequence |
|---|---|---|
| Benchmark, tier, group (category or subcategory), scenario entry | An opaque string held in `OpaqueId<Tag>` (`BenchmarkId`, `TierId`, `GroupId`, `ScenarioEntryId`); the tag types keep the four kinds from being assigned to one another ([`benchmark_ids.h`](../../../src/domain/benchmarks/benchmark_ids.h)) | Names, order and colours never participate in identity; see the paragraphs below the table. |
| Managed file in the snapshot | The `filename` within the managed directory; the entries are ordered by filename ([`benchmark_library_snapshot.h`](../../../src/data/interfaces/benchmark_library_snapshot.h)) | A file's embedded `BenchmarkId` is not the key. A new file is named `<benchmark id>.json` ([`BenchmarksService::writeAccepted`](../../../src/data/benchmarks_service.cpp)), but an existing file keeps its name whatever ID it holds. |
| Content of one file | SHA-256 hex digest of its bytes ([`benchmark_store.cpp`](../../../src/qt_data/benchmark_store.cpp), `digestOf`) | The digest is the optimistic-concurrency precondition for replacing or deleting that file. |
| Edit token | `{BenchmarkId, filename, digest}` ([`BenchmarkEditToken`](../../../src/data/interfaces/i_benchmarks_service.h)) | Admission is the service's responsibility: `acceptedEntry` honours a token only while the same filename still holds the same digest and the same embedded ID, and only for a loaded entry. |
| Scenario a definition entry targets | One or more scenario hashes: `ScenarioEntry::hash` plus `additionalHashes` ([`mappedScenarioHashes`](../../../src/domain/benchmarks/benchmark_resolution.cpp)); the first non-empty hash is primary; duplicate and empty hashes are dropped | `ScenarioId` equality is the hash alone ([Run history data][run-history-data]), so a hash is the stable key and the entry's `name` is a retained display label. |
| Scenario name used to find mapping candidates | `scenarioNameKey`: ASCII-lowercased, with ASCII whitespace and U+00A0 trimmed from both ends ([`benchmark_resolution.cpp`](../../../src/domain/benchmarks/benchmark_resolution.cpp)) | Finds same-name candidates even for mapped entries, so the editor can offer manual linked-version changes. Only entries with no mapped hash are eligible for automatic mapping. Non-ASCII case differences do not match. |
| Recent benchmark | The `BenchmarkId` string, in an ordered list | See [Recent benchmark IDs](#recent-benchmark-ids) for preference order and persistence. |

`BenchmarksService` returns an edit token on a successful write. Opening an entry, replacing it in the background and deleting an entry that is not open each construct a token from its loaded snapshot entry ([`openBenchmark`](../../../src/app/usecases/benchmark_manager_use_case.cpp), [`runReconcilePass`](../../../src/app/usecases/benchmark_resolution_use_case.cpp), `BenchmarkManagerViewModel::deleteBenchmark`).

Value equality is a separate, broader notion: the definition value types use defaulted comparison over all their fields, which the manager view model relies on for dirty detection (`*m_draft != *m_baseline`). A `Threshold` addresses its tier by `TierId`, not by ladder position, so reordering or renaming tiers keeps thresholds attached ([`benchmark.h`](../../../src/domain/benchmarks/benchmark.h)).

The domain editor delegates ID generation to an injected factory ([`benchmark_editor.h`](../../../src/domain/benchmarks/benchmark_editor.h)). The factory supplies IDs for new elements: the manager view model supplies its own UUID lambda for local additions, and `App::App()` supplies the manager use case's factory for new and imported seeds.

Two files may embed the same `BenchmarkId`; the snapshot allows it because it is keyed by filename. Consumers that address a benchmark by ID resolve the ambiguity differently:

| Consumer | Duplicate-ID outcome |
|---|---|
| Tracking | Treats the ID as unavailable (`findLoaded` in [`benchmark_tracking_use_case.cpp`](../../../src/app/usecases/benchmark_tracking_use_case.cpp); see [Benchmark tracking][benchmark-tracking]). |
| Manager | `BenchmarkManagerUseCase::openBenchmark` opens the first loaded match ([`benchmark_manager_use_case.cpp`](../../../src/app/usecases/benchmark_manager_use_case.cpp)). |
| Resolution map | Keeps the last result assigned to the ID while processing entries in filename order ([`benchmark_resolution_use_case.cpp`](../../../src/app/usecases/benchmark_resolution_use_case.cpp)). |

The code read here contains no check that prevents or reports duplicate IDs across files.

### Stored form and versions

Each benchmark is one JSON file in the managed directory, by default `<AppDataLocation>/benchmarks` (constructed in the `App` delegating constructor); a missing directory is created on scan or write ([`BenchmarkStore::scan`](../../../src/qt_data/benchmark_store.cpp)). The JSON root object has exactly the keys `schemaVersion`, `id`, `name`, `tiers`, `uncategorized`, `categories`; categories hold direct `scenarios` and `subcategories`; every object is decoded with an exact key set, so an unknown key invalidates the file rather than being preserved.

| Version | Entry hash field | Read | Written |
|---|---|---|---|
| 1 | `hash`: string or null | Accepted; decoded into `hash` | Never. Any write of the file emits version 2. |
| 2 (current, `kSchemaVersion`) | `hashes`: array of unique non-empty strings, primary first | Accepted | Always |
| greater than 2 | not decoded | `Unsupported` problem entry carrying the version | Never |

Loading a v1 file upgrades only its in-memory representation; its bytes remain unchanged. The next manager save or automatic-mapping write-back (below) encodes the definition as v2. Encoding canonicalises the hash fields, dropping empties and repeats and folding them into one `hashes` array. For values decoded from a supported v1 file this loses nothing the model represents: a null or empty `hash` already decodes as unmapped (`decodeEntry` and `encodeEntry` in [`benchmark_store.cpp`](../../../src/qt_data/benchmark_store.cpp)).

Files that cannot become a definition are classified, not dropped, in `classifyBytes`:

| Outcome | Cause |
|---|---|
| `Invalid` | File found during the directory scan that could not be opened (with an empty digest), JSON parse failure, root not an object, missing, non-numeric or non-integral `schemaVersion`, version below 1, any structural decode failure, a duplicated element ID anywhere in the definition, or a threshold naming a tier the definition does not declare |
| `Unsupported` | Integral `schemaVersion` greater than the supported one (compared after narrowing to `int`, so values outside the `int` range are not meaningfully classified) |

A problem entry retains a best-effort `displayName` and embedded `id` only if decoding reached a numeric, integral `schemaVersion`; earlier failures leave both empty. Tokened operations never rewrite or delete a problem entry: `BenchmarksService::acceptedEntry` honours a token only for a `LoadedBenchmark`, and the manager offers no delete for problem entries. A tokenless create does not consult the snapshot; the store's preflight refuses a file whose readable digest exists (see Authority). Recovering a problem file is a matter of editing or removing it outside the application and refreshing.

Completeness issues do not make a well-formed definition a problem entry: validation findings such as duplicate resolved hashes, missing thresholds or empty categories leave the file `Loaded` with `Completeness::Incomplete`.

### Validation and trackability

`validateBenchmark` ([`benchmark_validation.cpp`](../../../src/domain/benchmarks/benchmark_validation.cpp)) is pure and returns the full issue list; the definition is `Trackable` exactly when the list is empty. Save admission checks the name, while decoding checks the stored representation and validation checks trackability. `validateBenchmark` runs in the store at load, in the service when a write is accepted (to classify the value stored in memory, not to admit it), and in the manager's working copy after every edit.

Validation and save admission use the same blank-name rule (`EmptyName` on save). An `Incomplete` definition with a non-blank name can still be saved.

The service checks only that the name is not blank, encodes the value directly and does not apply the decoder's representation checks (exact shape, duplicate IDs, dangling tier references), so a malformed value submitted straight to the service can be accepted in memory and classified `Invalid` on the next refresh.

The trackability rules that carry consequences:

- A trackable benchmark has at least one tier and at least one scenario.
- Every tier, group, scenario and the benchmark itself needs a non-blank name; names of tiers and of scenario entries must be unique within the benchmark; no hash may be mapped by two entries.
- A category holds either direct scenarios or subcategories, never both and never neither; a subcategory needs scenarios.
- Each entry needs exactly one finite, non-negative threshold per tier, and its thresholds must increase strictly in the current tier order. The offending cell is addressable by entry and tier ID. Reordering tiers can therefore turn a trackable definition incomplete.

Evaluation produces aggregate (benchmark and group) ranks and rank histories only for trackable definitions; per-entry results, including normalised ranks for entries with a valid ladder, are still produced for incomplete ones ([Benchmark tracking][benchmark-tracking]).

### Authority

| Copy | Role | Authority |
|---|---|---|
| Files in the managed directory | Persisted form | The recovery source. Application code changes them only through `IBenchmarkStore::write`/`remove`; other programs may also change them, and a refresh accepts those changes. |
| Accepted snapshot in `BenchmarksService` | The application's view of the files | Authoritative for every consumer. Replaced wholesale by a successful `refresh()` scan, and patched per file after a confirmed write or delete. External changes enter the accepted snapshot through `refresh()`. The service refreshes at construction, and the manager exposes an explicit refresh command; nothing watches the directory (see Evidence and limitations). |
| Working copy in `BenchmarkManagerViewModel` | Unsaved edits | Never authoritative; submitted whole through a save and never rebased onto a changed snapshot ([Benchmark editing][benchmark-editing]). |
| `BenchmarkResolutionSnapshot`, `BenchmarkProjection`, `BenchmarkWorkspaceSnapshot` | Derived | Rebuilt on demand from the snapshot, the profile and configuration; never persisted. |

A failed directory scan keeps the last accepted snapshot, sets `lastRefreshFailed` and still notifies; a failed or conflicting write leaves accepted memory untouched.

Writes are conditional at preflight. Before writing, [`BenchmarkStore::write`](../../../src/qt_data/benchmark_store.cpp) reads the current file digest. A replacement requires the on-disk digest to match the admitted token's digest at preflight. A create is refused when a readable file already exists; a missing file, or an existing one that cannot be opened, yields no digest and the create proceeds. The bytes then go through `QSaveFile` (temporary sibling plus atomic commit).

The digest check and the commit or removal are two steps with no lock between them, so a change made after the check can still be overwritten; the precondition narrows the race rather than closing it.

A differing on-disk digest produces `Conflict`; a token that no longer matches an accepted entry produces `StaleToken`; an unknown filename produces `UnknownTarget` ([`BenchmarksService::writeAccepted`](../../../src/data/benchmarks_service.cpp)).

A batch replacement is not transactional: [`BenchmarksService::replaceBatch`](../../../src/data/benchmarks_service.cpp) writes requests independently and returns their outcomes in request order. It publishes at most once, with a single revision increment for that publication.

### Automatic mapping is a data mutation

An entry is linked automatically when it has no mapped hash, its name key matches exactly one profile scenario, and no other entry in the benchmark already maps that candidate hash: the entry receives the hash and also adopts the played scenario's exact name ([`computeAutoMappings`](../../../src/app/usecases/benchmark_resolution_use_case.cpp)). Within one pass the first eligible entry in traversal order claims a hash, so a later entry with the same candidate is not written. An entry with several candidates is `Ambiguous` and receives no automatic mapping. The states `Resolved`, `MappedUnavailable` (mapped but no mapped hash is in the profile), `Unresolved`, `Ambiguous` and `AutoMappable` are derived by `resolve` and are not stored. Derived status is separate from eligibility: `AutoMappable` is assigned from the candidate count alone, without the reservation check, so an entry can be labelled `AutoMappable` yet not be written because another entry claimed the hash.

For each benchmark without the edit lease, the resolution use case persists these links as a token-addressed `replaceBatch`, so a profile change can rewrite a definition file, change its digest and bump the library revision with no user action; the write is subject to the digest precondition above. When the write is scheduled, which benchmark the lease exempts, and how a failure is recorded and retried are in [Benchmark tracking][benchmark-tracking]; why the editor links on save instead is in [Benchmark editing][benchmark-editing].

### Recent benchmark IDs

`RecentBenchmarks` keeps `BenchmarkId` strings in selection-preference order, loaded from and saved to `ISettingsService::{get,set}RecentBenchmarkIds` ([`recent_benchmarks.cpp`](../../../src/app/usecases/recent_benchmarks.cpp)). `SettingsService` stores it under `benchmarks/recentIds`, falls back to a single legacy `benchmarks/lastSelectedId` value when the new key is absent, and removes the legacy key on the first write ([`settings_service.cpp`](../../../src/qt_data/settings_service.cpp)). The list has no size limit and retains IDs of deleted benchmarks; it is persistent selection memory, not a benchmark record. The two insertion rules differ: `markViewed` moves an ID to the front, whereas `markCreated` inserts a not-yet-listed ID behind the current front so creating a benchmark does not displace what the user was viewing. The list is therefore not chronological; tracking uses it to choose its default selection ([Benchmark tracking][benchmark-tracking]).

## Consequences and invariants

- **Identity never derives from a mutable attribute.** Thresholds reference `TierId`, selections reference entry and tier IDs, and files are keyed by filename. Replacing an ID with a name or a position would let a rename or reorder retarget a threshold or an editor selection ([ADR 0002](../decisions/0002-embedded-stable-identity-for-benchmark-elements.md)). The code agrees with the ADR; its evidence links name two paths that have since moved (`benchmark_repository.cpp` and `app/contracts/benchmark_library_snapshot.h`).
- **Tokened writes only touch loaded files.** The mechanism is that `BenchmarksService::acceptedEntry` admits a token only for a `LoadedBenchmark` whose filename, digest and embedded `BenchmarkId` match the token. A change that admitted tokens for problem entries would let an automatic or manual write replace a file the application could not read. The tokenless create path is guarded only by the store's digest preflight.
- **A write needs the digest it was admitted under.** Removing the preflight comparison would allow saves and automatic write-back to overwrite an external edit already present at preflight. The remaining race is described under [Authority](#authority).
- **Automatic mapping uses names to discover hashes.** Name matching may find candidates for any entry (the editor offers them for manual changes), but an automatic link is allowed only for an entry with no mapped hash and only when the candidate is unique; making it tolerant of several candidates would silently attach an entry to an arbitrary scenario.
- **Schema writes are one-way.** Because v1 files are only ever rewritten as v2, an older build reading a rewritten file reports it `Unsupported`. When raising `kSchemaVersion`, retain a decoder for each earlier version the application continues to support; this is a maintenance requirement, not a promise to read every file left on disk.
- **Duplicate IDs across files are unmanaged.** Changes to ID-based lookup must preserve or explicitly resolve the differing consumer outcomes described under [Identity](#identity).

## Evidence and limitations

Verified by reading `benchmark_ids.h`, `benchmark.h`, `benchmark_resolution.{h,cpp}`, `benchmark_validation.{h,cpp}`, `benchmark_editor.h`, `BenchmarksService`, the store and snapshot interfaces, the complete `BenchmarkStore` implementation, `RecentBenchmarks`, the recent-ID functions of `SettingsService`, and the resolution use case. The statement that no code watches the benchmark directory (the only `QFileSystemWatcher` in `src` belongs to `FileService`) rests on a text search for `QFileSystemWatcher` and for calls to `refresh()`, not a full call-graph proof. The mixed-version behaviour of older builds is inferred from the version check, not tested. No migration test fixtures or on-disk samples were inspected. Unreadable files are classified `Invalid` with an empty digest; whether the manager can still list and refresh past one was read from code, not exercised.

<!-- arch-doc:references:start -->
[benchmark-editing]: benchmark-editing.md
[benchmark-tracking]: benchmark-tracking.md
[benchmarks]: benchmarks.md
[graph-configuration]: graph-configuration.md
[run-history-data]: run-history-data.md
<!-- arch-doc:references:end -->
