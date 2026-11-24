---
type: internal
area: Build & packaging
---
Every build tree now lives under the git-ignored `build/` directory instead of sibling `build-*` and `cmake-build-*` folders: `build/script-build` (Debug runner), `build/release` and `build/measure` (`package-release.ps1`), `build/clion/<profile>` (CLion profiles) and `build/qtc/<profile>` (Qt Creator presets).
`build_runner` (`cli.py`, `config.safe_remove_build_tree`, `orchestrator.py`), `ui_automation/cli.py`'s driver path, the Zed tasks, the build docs and the runner tests follow the new `build/script-build`. The `.temp/build-agent.lock` lock file keeps its name.
Existing trees at the old locations are no longer used and can be deleted; a build tree is not relocatable, so the new ones configure from scratch.
