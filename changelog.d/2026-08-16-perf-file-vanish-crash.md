---
type: fixed
area: Data & Profile
user: Fixed a crash that could happen right as a scenario run's stats file appeared on disk.
---
When KovaaKs renamed or deleted a newly discovered `.perf` file before it could be decoded, `ProtoDecoder::decode_file` threw an exception that escaped the watcher or profile-build path. `ProfileService` and `ProfileBuilder` now skip and log that unavailable file so the app continues processing the remaining runs.
