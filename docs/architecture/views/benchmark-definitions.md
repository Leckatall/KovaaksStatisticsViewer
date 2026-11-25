---
id: benchmark-definitions
name: Benchmark definitions
viewpoints: [information, runtime]
---

# Benchmark definitions

## Concern and scope

What identifies a benchmark or one of its elements? Which copy is authoritative, and how does the application load, upgrade or classify its files? This matters to anyone changing an ID, the JSON shape, a write path, or the rules that decide whether a definition can be tracked.

The view covers a definition from its stored file through the accepted in-memory snapshot to the derived scenario resolution, and the recent benchmark filenames kept in settings. This view expands the benchmark data described in [Benchmarks][benchmarks]. [Benchmark tracking][benchmark-tracking] explains change publication; [Benchmark editing][benchmark-editing] explains working-copy edits and saves. Run and scenario identity belong to [Run history data][run-history-data]; the stored form of graph settings (including the benchmark graphs) belongs to [Graph configuration][graph-configuration].

## Architectural account

### Identity

| Entity | Identity or lookup key | Consequence |
|---|---|---|
| Benchmark | Its `filename` within the managed directory; the entries are ordered by filename in `std::string` order ([`benchmark_library_snapshot.h`](../../../src/data/interfaces/benchmark_library_snapshot.h)) | Two files are always two benchmarks, whatever they contain. The benchmark's name is the filename without its final `.json` (matched ignoring ASCII case); no name or ID is stored in the file. A file renamed or copied outside the application is a removed benchmark and a new one. |
| Benchmark in memory | `BenchmarkId`, a key `BenchmarksService` mints per filename for the session (`benchmark-file-N`), set on the entry and on a loaded definition ([`benchmarks_service.cpp`](../../../src/data/benchmarks_service.cpp)) | Kept across refreshes while the filename is present, moved to the new filename by a renaming save, and dropped when the filename disappears, so a file that comes back gets a new key. A create admits the file under the draft's own ID. Never persisted. |
| Tier, group (category or subcategory), scenario entry | An opaque string held in `OpaqueId<Tag>` (`TierId`, `GroupId`, `BenchmarkScenarioId`); the tag types keep the kinds from being assigned to one another ([`benchmark_ids.h`](../../../src/domain/benchmarks/benchmark_ids.h)) | Names, order and colours never participate in identity; see the paragraphs below the table. |
| Content of one file | SHA-256 hex digest of its bytes ([`benchmark_store.cpp`](../../../src/qt_data/benchmark_store.cpp), `digestOf`) | The digest is the optimistic-concurrency precondition for replacing or deleting that file. |
| Edit token | `{BenchmarkId, filename, digest}` ([`BenchmarkEditToken`](../../../src/data/interfaces/i_benchmarks_service.h)) | Admission is the service's responsibility: `acceptedEntry` honours a token only while the same filename still holds the same digest under the same key, and only for a loaded entry. |
| Scenario a definition entry targets | The entry's single `hashes` list ([`BenchmarkScenario`](../../../src/domain/benchmarks/benchmark_definition.h)): unique, non-empty, in user order, with no primary hash. The type does not enforce this, its writers do, and validation reports a hash held by two entries as `DuplicateResolvedHash`. `BenchmarkDefinition::scenarioHashes` yields the unique hashes in first-occurrence order | `ScenarioId` equality is the hash alone ([Run history data][run-history-data]), so a hash is the stable key and the entry's `name` is a retained display label. |
| Scenario name used to find mapping candidates | `scenarioNameKey`: ASCII-lowercased, with ASCII whitespace and U+00A0 trimmed from both ends ([`benchmark_resolution.cpp`](../../../src/domain/benchmarks/benchmark_resolution.cpp)) | Finds same-name candidates even for mapped entries, so the editor can offer manual linked-version changes. Only entries with no mapped hash are eligible for automatic mapping. Non-ASCII case differences do not match. |
| Recent benchmark | The filename, in an ordered list | See [Recent benchmarks](#recent-benchmarks) for preference order and persistence. |

`BenchmarksService` returns an edit token on a successful write. Other operations construct a token from a loaded entry in the snapshot: [`openBenchmark`](../../../src/app/usecases/benchmark_manager_use_case.cpp) when opening an entry, [`runReconcilePass`](../../../src/app/usecases/benchmark_resolution_use_case.cpp) when replacing it in the background, and `BenchmarkManagerViewModel::deleteBenchmark` when deleting an entry that is not open.

Value equality is a separate, broader notion: the definition value types use defaulted comparison over all their fields, which the manager view model relies on for dirty detection (`*m_draft != *m_baseline`). A `Threshold` addresses its tier by `TierId`, not by ladder position, so reordering or renaming tiers keeps thresholds attached ([`benchmark_definition.h`](../../../src/domain/benchmarks/benchmark_definition.h)).

The domain editor delegates ID generation to an injected factory ([`benchmark_editor.h`](../../../src/domain/benchmarks/benchmark_editor.h)). The factory supplies IDs for new elements: the manager view model supplies its own UUID lambda for local additions, and `App::App()` supplies the manager use case's factory for new and imported seeds.

A scan gives every newly seen filename, problem entries included, a distinct service-minted key. A tokenless create does not mint one: it admits the file under the caller's `benchmark.id` without checking it against other entries, and the manager use case takes new and imported IDs from the injected factory (`App::App()` supplies a UUID factory). Uniqueness of `BenchmarkId` across accepted entries therefore holds only while every create path supplies a fresh ID; resolution maps, selection and the edit lease address entries by that key, so a duplicate would make them find the wrong entry. Replacing the factory or adding another create path must preserve this.

### Benchmark names

The naming rule is Qt-free and general to any file the application names ([`file_naming.h`](../../../src/data/files/file_naming.h), namespace `files::`); `.json` and the `Benchmark` fallback are arguments. `checkName` reports the first broken rule: blank, an invalid character (`< > : " / \ | ? *` or U+0000–U+001F), longer than 255 UTF-16 units less the extension (250 for `.json`), a trailing dot or space, or a reserved device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`, alone or followed by `.`). `cleanName` makes any stored name usable (removes invalid characters, truncates without splitting a surrogate pair, trims trailing dots and spaces, then falls back), and `uniqueName` appends ` (N)` with the smallest free N, shortening the name to fit. Callers compare names ignoring letter case with Qt: `QString::toCaseFolded` in the store, `Qt::CaseInsensitive` in `BenchmarkManagerUseCase`.

The editor refuses an unusable or taken name rather than cleaning it: `BenchmarkManagerUseCase::checkName` maps `files::checkName` and adds `Taken` when any other library entry, problem entries included, has the same folded name; the editor's own file is excluded, so a case-only rename is accepted ([Benchmark editing][benchmark-editing]).

### Stored form and versions

Each benchmark is one JSON file in the managed directory. The `App` delegating constructor sets the default directory to `<AppDataLocation>/benchmarks`; the store creates it on scan or write if it is missing ([`BenchmarkStore::scan`](../../../src/qt_data/benchmark_store.cpp)).

The current format is schema 3. Its JSON root object has exactly four keys: `schemaVersion`, `tiers`, `uncategorized` and `categories`. Categories hold direct `scenarios` and `subcategories`; scenario entries are `{id, name, hashes, thresholds}`. The decoder requires an exact key set for every object, so an unknown key invalidates the file rather than being preserved.

| Version | Shape | Read | Written |
|---|---|---|---|
| 1 | top-level `id` and `name`; entry `hash`: string or null | Migrated on scan | Never |
| 2 | top-level `id` and `name`; entry `hashes`: array of unique non-empty strings | Migrated on scan | Never |
| 3 (current, `kSchemaVersion`) | no top-level `id` or `name` | `readCurrentFormat` | Always |
| greater than 3 | not decoded | `Unsupported` problem entry carrying the version | Never |

`readStoredBenchmark` is the one place that branches on the version: it hands version 3 to `readCurrentFormat`, which alone builds a `BenchmarkDefinition` and knows no other version, and versions 1 and 2 to migration ([ADR 0011](../decisions/0011-older-stored-formats-are-migrated-outside-current-format-code.md)).

### Migration on scan

Every scan migrates every schema 1 and 2 file before it returns, so neither the service nor any consumer ever sees an older format. The steps live in [`benchmark_migration.cpp`](../../../src/qt_data/benchmark_migration/benchmark_migration.cpp): `migrateV1ToV2` (a single `hash` becomes a `hashes` list) and `migrateV2ToV3` (drops `id` and `name`, returning the stored name). Each takes and returns JSON, checks that its input is exactly its source version's shape, and builds no domain type ([ADR 0012](../decisions/0012-migrations-produce-current-stored-data-never-the-in-memory-model.md), [ADR 0013](../decisions/0013-migrations-upgrade-stored-data-one-version-at-a-time.md)). The upgraded JSON must then pass `readCurrentFormat`.

The scan names migrated files after their stored names. It starts with every enumerated filename marked as taken, then processes migrating files in ascending order of their case-folded filenames. Each file claims `uniqueName(cleanName(storedName))`, ignoring only its own current name. Each claimed name is added to the taken set; a name another migrating file is leaving stays taken for the rest of the scan.

The scan renames each file before rewriting it through `QSaveFile`. If renaming fails, the original bytes remain under the old name. If writing fails, they remain under the new name. A failure never leaves new content under the old name. A file already named after its stored name is rewritten in place.

A file whose migration does not complete is a `MigrationFailed` problem entry carrying `{fromVersion, reason}`: `SourceMismatch` at the step whose input was malformed, or at the 2→3 step `InvalidResult`, `WriteFailed` or `RenameFailed`. The next scan retries it. Presentation shows `Migration from version x to version x+1 failed: <reason>` ([`benchmark_format.cpp`](../../../src/ui/presentation/benchmarks/benchmark_format.cpp)). Invalid and Unsupported files are never written or renamed.

Files that cannot become a definition are classified, not dropped:

| Outcome | Cause |
|---|---|
| `Invalid` | File found during the directory scan that could not be opened (with an empty digest), JSON parse failure, root not an object, missing, non-numeric or non-integral `schemaVersion`, version below 1, any structural decode failure, a duplicated element ID anywhere in the definition, or a threshold naming a tier the definition does not declare |
| `Unsupported` | Integral `schemaVersion` greater than the supported one. The comparison is made on the parsed `double`; only the version carried in the problem entry's payload is narrowed to `int`, so for a value outside the `int` range the file is still `Unsupported` but the reported version is not meaningful |
| `MigrationFailed` | A schema 1 or 2 file whose migration did not complete (above) |

A problem entry is named after its file like any other entry. Tokened operations never rewrite or delete a problem entry: `BenchmarksService::acceptedEntry` honours a token only for a `LoadedBenchmark`, and the manager offers no delete for problem entries. A tokenless create does not consult the snapshot; the store's preflight refuses a file whose readable digest exists (see Authority). Recovering a problem file is a matter of editing or removing it outside the application and refreshing.

Completeness issues do not make a well-formed definition a problem entry: validation findings such as duplicate resolved hashes, missing thresholds or empty categories leave the file `Loaded` with `Completeness::Incomplete`.

### Validation and trackability

`validateBenchmark` ([`benchmark_validation.cpp`](../../../src/domain/benchmarks/benchmark_validation.cpp)) is pure and returns the full issue list; the definition is `Trackable` exactly when the list is empty. The name check gates saving, while decoding checks the stored representation and validation checks trackability. `validateBenchmark` runs in the store at load, again in the service once it has named the definition after its file, in the service when a write is accepted (to classify the value stored in memory, not to admit it), and in the manager's working copy after every edit.

Validation's blank-name rule matches the name check's. An `Incomplete` definition with a usable, unused name can still be saved.

The service performs no name check of its own: its save contract requires a name the caller has checked. It encodes the value directly and does not apply the decoder's representation checks (exact shape, duplicate IDs, dangling tier references), so a malformed value submitted straight to the service can be accepted in memory and classified `Invalid` on the next refresh.

The trackability rules that carry consequences:

- A trackable benchmark has at least one tier and at least one scenario.
- Every tier, group, scenario and the benchmark itself needs a non-blank name; names of tiers and of scenario entries must be unique within the benchmark; no hash may be mapped by two entries.
- A category holds either direct scenarios or subcategories, never both and never neither; a subcategory needs scenarios.
- Each entry needs exactly one finite, non-negative threshold per tier, and its thresholds must increase strictly in the current tier order. The offending cell is addressable by entry and tier ID. Reordering tiers can therefore turn a trackable definition incomplete.

Evaluation produces aggregate (benchmark and group) ranks and rank histories only for trackable definitions; per-entry results, including normalised ranks for entries with a valid ladder, are still produced for incomplete ones ([Benchmark evaluation][benchmark-evaluation]).

### Authority

| Copy | Role | Authority |
|---|---|---|
| Files in the managed directory | Persisted form | The recovery source. Application code changes them only through `IBenchmarkStore::write`/`rename`/`remove` and the store's own migration on scan; other programs may also change them, and a refresh accepts those changes. |
| Accepted snapshot in `BenchmarksService` | The application's view of the files | Authoritative for every consumer. Replaced wholesale by a successful `refresh()` scan, and patched per file after a confirmed write or delete. External changes enter the accepted snapshot through `refresh()`. The service refreshes at construction, and the manager exposes an explicit refresh command; nothing watches the directory (see Evidence and limitations). |
| Working copy in `BenchmarkManagerViewModel` | Unsaved edits | Never authoritative; submitted whole through a save and never rebased onto a changed snapshot ([Benchmark editing][benchmark-editing]). |
| `BenchmarkResolutionSnapshot`, `BenchmarkWorkspaceSnapshot` (with the evaluated `domain::Benchmark` it carries) | Derived | Rebuilt on demand from the snapshot, the profile and configuration; never persisted. |

A failed directory scan keeps the last accepted snapshot, sets `lastRefreshFailed` and still notifies. A write that fails or conflicts before its content is written leaves accepted memory untouched. `RenameFailed` is the exception: the content write has succeeded, so the service admits and publishes it under the old filename before reporting the error (see below).

Writes are conditional at preflight. Before writing, [`BenchmarkStore::write`](../../../src/qt_data/benchmark_store.cpp) reads the current file digest. A replacement requires the on-disk digest to match the admitted token's digest at preflight. A create is refused when a readable file already exists; a missing file, or an existing one that cannot be opened, yields no digest and the create proceeds. The bytes then go through `QSaveFile` (temporary sibling plus atomic commit).

The digest check and the commit or removal are two steps with no lock between them, so a change made after the check can still be overwritten; the precondition narrows the race rather than closing it.

A differing on-disk digest produces `Conflict`; a token that no longer matches an accepted entry produces `StaleToken`; an unknown filename produces `UnknownTarget` ([`BenchmarksService::save`](../../../src/data/benchmarks_service.cpp)).

A save writes `<name>.json`. A replace whose name differs from its file's stem first writes the old file under its digest and then calls `IBenchmarkStore::rename` under the new digest; the rename refuses a missing or changed source and an existing target that is another file, and allows a case-only respelling. On success the entry and its key move to the new filename. If the rename fails, the new content stays under the old filename and the outcome is `RenameFailed` carrying a token for that content, so a retried save is not stale and renames.

A batch replacement is not transactional: [`BenchmarksService::replaceBatch`](../../../src/data/benchmarks_service.cpp) writes requests independently, each in place under its token's filename (never renaming or creating), and returns their outcomes in request order. It publishes at most once, with a single revision increment for that publication.

### Automatic mapping is a data mutation

Automatic mapping requires an entry with no mapped hash, exactly one profile scenario matching its name key, and no other entry in the benchmark already mapping that candidate hash. The entry receives the hash and adopts the played scenario's exact name ([`computeAutoMappings`](../../../src/app/usecases/benchmark_resolution_use_case.cpp)). Within one pass, the first eligible entry in traversal order claims a hash; a later entry with the same candidate is not written. An entry with several candidates is `Ambiguous` and receives no automatic mapping.

Resolution status does not guarantee that an automatic mapping will be written. `resolve` derives the states `Resolved`, `MappedUnavailable` (mapped but no mapped hash is in the profile), `Unresolved`, `Ambiguous` and `AutoMappable`; none is stored. It assigns `AutoMappable` from the candidate count alone, without checking whether another entry has claimed the hash. An entry can therefore be labelled `AutoMappable` yet receive no write.

For each benchmark without the edit lease, the resolution use case persists these links as a token-addressed `replaceBatch`, so a profile change can rewrite a definition file, change its digest and bump the library revision with no user action; the write is subject to the digest precondition above. When the write is scheduled, which benchmark the lease exempts, and how a failure is recorded and retried are in [Benchmark tracking][benchmark-tracking]; why the editor links on save instead is in [Benchmark editing][benchmark-editing].

### Recent benchmarks

`RecentBenchmarks` keeps benchmark filenames in selection-preference order, loaded from and saved to `ISettingsService::{get,set}RecentBenchmarkFiles` ([`recent_benchmarks.cpp`](../../../src/app/usecases/recent_benchmarks.cpp)). `SettingsService` stores it under `benchmarks/recentFiles` and reads nothing else; values left under the older `benchmarks/recentIds` and `benchmarks/lastSelectedId` keys are neither read nor removed ([`settings_service.cpp`](../../../src/qt_data/settings_service.cpp)). The list has no size limit and retains filenames of deleted benchmarks; it is persistent selection memory, not a benchmark record. The insertion rules differ: `markViewed` moves a filename to the front, whereas `markCreated` inserts a not-yet-listed filename behind the current front so creating a benchmark does not displace what the user was viewing; `rename`, called by the manager after a renaming save, replaces a filename where it stands. The list is therefore not chronological; tracking uses it to choose its default selection ([Benchmark tracking][benchmark-tracking]).

## Consequences and invariants

- **Element identity never derives from a mutable attribute.** Thresholds reference `TierId`, and selections reference entry and tier IDs. Replacing an element ID with a name or a position would let a rename or reorder retarget a threshold or an editor selection ([ADR 0002](../decisions/0002-embedded-stable-identity-for-benchmark-elements.md)). At the benchmark level the filename is the identity ([ADR 0014](../decisions/0014-a-benchmarks-filename-is-its-identity.md), amending ADR 0002); the in-memory key follows a rename made through the application and nothing else.
- **Tokened writes only touch loaded files.** The mechanism is that `BenchmarksService::acceptedEntry` admits a token only for a `LoadedBenchmark` whose filename, digest and key match the token. A change that admitted tokens for problem entries would let an automatic or manual write replace a file the application could not read. The tokenless create path is guarded only by the store's digest preflight.
- **A write needs the digest it was admitted under.** Removing the preflight comparison would allow saves and automatic write-back to overwrite an external edit already present at preflight. The remaining race is described under [Authority](#authority).
- **Automatic mapping uses names to discover hashes.** Name matching may find candidates for any entry (the editor offers them for manual changes), but an automatic link is allowed only for an entry with no mapped hash and only when the candidate is unique; making it tolerant of several candidates would silently attach an entry to an arbitrary scenario.
- **Schema writes are one-way.** Every write and every migration emits schema 3, which an older build reports `Unsupported`. Raising `kSchemaVersion` means adding one JSON-to-JSON step from the previous version and keeping `readCurrentFormat` to the new version only.
- **Migration never loses a stored name.** It renames before it rewrites; reversing the order would leave schema 3 content under a UUID filename after a failed rename, which no later scan would migrate.

## Evidence and limitations

Verified by reading `benchmark_ids.h`, `benchmark_definition.h`, `benchmark_resolution.{h,cpp}`, `benchmark_validation.{h,cpp}`, `benchmark_editor.h`, `BenchmarksService`, the store and snapshot interfaces, the complete `BenchmarkStore` implementation and its migration steps, `file_naming`, `RecentBenchmarks`, the recent-benchmark functions of `SettingsService`, and the resolution use case. The statement that no code watches the benchmark directory (the only `QFileSystemWatcher` in `src` belongs to `FileService`) rests on a text search for `QFileSystemWatcher` and for calls to `refresh()`, not a full call-graph proof. The mixed-version behaviour of older builds is inferred from the version check, not tested. Unreadable files are classified `Invalid` with an empty digest; whether the manager can still list and refresh past one was read from code, not exercised.

<!-- arch-doc:references:start -->
[benchmark-editing]: benchmark-editing.md
[benchmark-evaluation]: benchmark-evaluation.md
[benchmark-tracking]: benchmark-tracking.md
[benchmarks]: benchmarks.md
[graph-configuration]: graph-configuration.md
[run-history-data]: run-history-data.md
<!-- arch-doc:references:end -->
