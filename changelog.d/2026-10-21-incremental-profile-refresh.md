---
type: added
area: Data & Profile
user: The profile now catches up on runs played while the app was closed, with Refresh Profile available in File and Settings.
---
`ProfileImporter` shares incremental source discovery across startup catch-up, manual refresh, and coalesced performances/stats notifications. Recorded source references skip decoding known components; late perf/CSV siblings enrich one logical run, with canonical perf timing replacing provisional CSV timing while retaining CSV totals and archived history.
`ProfileService` accepts immutable worker candidates through monotonic profile/settings tokens, saves each changed batch once, and publishes only after persistence succeeds. Unchanged refreshes do not save or publish. Session-local source aliases suppress repeated reads of duplicate files across roots without a profile-schema change.
`SessionController` serializes refresh/rebuild requests, preserves manual and intervening startup selections, follows new live runs, and bounds incomplete-source retries. `FileService` watches directories and compares file-name listings, fingerprinting and individually watching only new or retryable files until they are accepted.
`SessionViewModel` supplies shared progress, completion counts, source diagnostics, and persistence-failure state to File/F5, Settings, and the main window. One whole-profile save still runs on the owning thread per changed batch; modifying previously imported source content is outside the incremental-refresh contract.
