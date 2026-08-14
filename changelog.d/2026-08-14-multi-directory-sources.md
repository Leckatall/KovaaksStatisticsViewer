---
type: changed
area: Data & Profile
user: Profiles can ingest runs from multiple configured KovaaKs directories. Existing profiles are rebuilt from disk on first launch, so runs whose `.perf` files are gone will not survive the upgrade.
---
Profiles now persist an immutable source-directory registry. Each run stores a directory id and filename instead of duplicating an absolute path, and settings retain the legacy single-directory value while supporting a list internally.
