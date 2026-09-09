---
type: changed
area: Build
---
`build_and_test.py` now takes an exclusive OS-level lock on the shared `build-agent/` tree
(`.temp/build-agent.lock`) before touching it. A second concurrent run fails fast with
`FAILED (runner)` instead of corrupting the tree. New `build_runner/locking.py` (`build_tree_lock`,
`BuildLockBusy`); wired into `cli.main()`. The lock is advisory and released automatically when the
holder dies, so there is no stale-lock state.
