---
type: internal
area: Build & packaging
---
The tracked toolchain env files (`toolchain.defaults.env`, `toolchain.mingw.env`, `toolchain.msvc.env`) now hold `<...>` placeholders instead of the maintainer's machine paths; each profile keeps its vcpkg triplet.
New ignored layer `scripts/toolchain.local.<flavour>.env`, read after `toolchain.local.env` and before the process environment, by both `build_runner/config.py` and `toolchain-config.ps1`. It exists so a machine can hold MinGW and MSVC paths side by side: a single local file sits above both profiles, so its compiler paths would leak into whichever flavour is selected.
Like `toolchain.local.env`, a linked worktree without its own copy reads the main worktree's (`find_local_env` now takes the file name).
Any required value still containing `<` or `>` fails resolution with the key name and the local files to set it in; neither character is legal in a Windows path.
