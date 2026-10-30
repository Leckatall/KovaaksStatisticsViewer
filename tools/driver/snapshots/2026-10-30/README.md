# Saved settings snapshot: 30 October 2026

`settings.ini` is an exact copy of the current app's saved INI settings. `overlay/` contains the stored profile and four benchmark definitions. This captures saved state; unsaved dialog edits and source `.perf` / stats files are not included.

From the repository root, prepare a new disposable session and start it later:

```powershell
& tools/driver/snapshots/2026-10-30/prepare.ps1
uv run python scripts/ui_driver.py --session snapshot-2026-10-30 batch tools/driver/snapshots/2026-10-30/start.txt
```

The default session has already been prepared. The preparation script refuses to overwrite a session; use `-Session another-name` for a fresh copy, and pass that name to the driver. Preparation copies the INI verbatim except for replacing its `file` section with paths inside the disposable session. The replay batch uses `--keep` so the driver retains the prepared data and settings, and restores the saved main-window dimensions.

Capture named shots in this session and use its `report` command when a report is wanted. Existing `full`, `smoke` and `unreachable` tours start their own synthetic datasets; they do not automatically use this snapshot.

The older `take_snapshot.py` helper reads registry settings. This snapshot uses the current INI settings instead. `manifest.json` records SHA-256 hashes for the archived settings, profile and benchmark files.
