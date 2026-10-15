---
type: added
area: Benchmarks
user: Link multiple versions of a scenario to one benchmark row to combine their scores, playtime, and progress history.
---
Benchmark entries retain an explicit primary mapping plus additional hashes; name matches remain suggestions once an entry is mapped. The manager's mapping panel can link same-name played versions and unlink individual hashes, including unavailable ones, through the existing draft, undo and save lifecycle.
Resolution accepts any available linked version without dropping missing mappings. Tracking queries every selected hash, and evaluation merges histories chronologically for personal bests and recent averages, includes each hash in progression, and counts overlapping hashes only once in benchmark playtime totals.
BenchmarkStore reads legacy schema 1 single-hash files and writes schema 2 hashes arrays, including nested entries. Raw profile runs and scenario identities remain intact.
