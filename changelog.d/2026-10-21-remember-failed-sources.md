---
type: changed
area: Data & Profile
user: Files that fail to import are remembered and are no longer retried every time the app starts or a new run arrives; Refresh Profile and Rebuild still retry them, and a file is retried automatically once it changes.
---
- `domain::FailedSource` (file ref, size, modified time, message) is held on `UserProfile` via `failedSources()`/`setFailedSources()` and persisted as `Store.failed_sources`. No `kStoreVersion` bump: the field is additive, so older v4 stores load with an empty list; entries whose directory id no longer resolves are dropped on load.
- `PerfFile`/`StatsFile` carry a `size`/`modified_ms` fingerprint, filled by `FileService::scanSources` with the same values as `fingerprintOf`.
- `ProfileImportRequest::skip_unchanged_failures` is set by `ProfileService::beginImport` for Refresh imports with reason Startup, Live or SourceSettingsChanged. `ProfileImporter` then leaves a recorded failure with an unchanged fingerprint out of the import and reports it again from the stored message.
- New `ProfileImportReason::Retry` for the controller's backoff timer. It must not skip, since the backoff exists to re-read an unchanged file after a transient error; Retry completions keep backing off within the existing three-attempt budget.
- `ProfileImportResult` returns the complete new `failed_sources` list and `failures_changed`. `acceptImport` saves a failures-only result but only calls `notifyProfileChanged()` when runs changed, so graphs don't reload. Records for files that left the scan or now import cleanly are pruned.
- `ProfileImportIssue::previously_failed` marks a skipped failure (never retryable); `SessionViewModel::profileImportIssues()` exposes it as `previouslyFailed`.
- A Live request arriving while a retry is pending, queued, or a Live/Retry import is in flight is promoted to `Retry` in `SessionController::refreshProfile` and keeps the backoff budget, so a run whose `.csv` lands just after its `.perf` failed transiently does not skip that unchanged `.perf`.
- A conflicting perf/csv group reports and records both files, so the csv cannot later merge alone into the run the pair was rejected against.
