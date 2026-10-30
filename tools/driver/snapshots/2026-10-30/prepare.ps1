param([string]$Session = 'snapshot-2026-10-30')

$ErrorActionPreference = 'Stop'
if ($Session -notmatch '^[A-Za-z0-9_-]+$') { throw 'Use letters, numbers, underscores or hyphens for the session name.' }
$snapshotRoot = $PSScriptRoot
$repositoryRoot = (Resolve-Path (Join-Path $snapshotRoot '../../../..')).Path
$sessionRoot = Join-Path $repositoryRoot ".temp/ui-driver/$Session"
if (Test-Path -LiteralPath $sessionRoot) { throw "Session already exists: $sessionRoot. Choose another -Session name." }

$dataRoot = Join-Path $sessionRoot 'data'
$settingsRoot = Join-Path $sessionRoot 'settings/Lecka'
New-Item -ItemType Directory -Path $dataRoot, $settingsRoot, (Join-Path $dataRoot 'FPSAimTrainer/performances'), (Join-Path $dataRoot 'FPSAimTrainer/stats') | Out-Null
Copy-Item -LiteralPath (Join-Path $snapshotRoot 'overlay/profile.pb') -Destination (Join-Path $dataRoot 'profile.pb')
Copy-Item -LiteralPath (Join-Path $snapshotRoot 'overlay/benchmarks') -Destination (Join-Path $dataRoot 'benchmarks') -Recurse
Set-Content -LiteralPath (Join-Path $dataRoot '.ksv-driver-dataset') -Value 'empty' -NoNewline -Encoding utf8

# Preserve Qt's INI encoding, including QVariant values, while redirecting source paths into the session.
$ini = [IO.File]::ReadAllText((Join-Path $snapshotRoot 'settings.ini'))
$ini = [regex]::Replace($ini, '(?ms)^\[file\]\r?\n.*?(?=^\[|\z)', '')
$dataUrl = ([Uri]($dataRoot + [IO.Path]::DirectorySeparatorChar)).AbsoluteUri.TrimEnd('/')
$profileUrl = ([Uri](Join-Path $dataRoot 'profile.pb')).AbsoluteUri
$ini += "`n[file]`nkovaaksDirs=$dataUrl`nprofilePath=$profileUrl`n"
[IO.File]::WriteAllText((Join-Path $settingsRoot 'KovaaksStatsViewer.ini'), $ini, [Text.UTF8Encoding]::new($false))
Write-Output "Prepared $sessionRoot"
Write-Output "uv run python scripts/ui_driver.py --session $Session batch tools/driver/snapshots/2026-10-30/start.txt"
