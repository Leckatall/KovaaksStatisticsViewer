<#
.SYNOPSIS
    Prints the project version, e.g. 0.4.0-alpha.

.DESCRIPTION
    CMakeLists.txt is the single source of the version. This script is the single *reader* of it, so
    that packaging, tagging and release automation never grow their own copy of the parsing and drift
    apart. It reads the numeric fields from project(... VERSION x.y.z) and appends KSV_VERSION_SUFFIX
    if one is set.

    Writes the version to stdout and nothing else, so it can be captured directly:
        $v = & ./scripts/version.ps1

.EXAMPLE
    ./scripts/version.ps1
#>

[CmdletBinding()]
param(
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"
trap { Write-Error $_; exit 1 }

# Not a param default: `powershell -File` evaluates those before $PSScriptRoot is populated, so
# "$PSScriptRoot/.." would silently resolve to the root of the current drive.
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path "$PSScriptRoot/..").Path }

$CMakeLists = Join-Path $RepoRoot "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $CMakeLists)) {
    throw "CMakeLists.txt not found at: $CMakeLists"
}

$Content = Get-Content -Raw -LiteralPath $CMakeLists

if ($Content -notmatch 'project\s*\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "Could not parse project(... VERSION x.y.z) from $CMakeLists"
}
$Version = $Matches[1]

if ($Content -match 'set\s*\(\s*KSV_VERSION_SUFFIX\s+"([^"]*)"\s*\)') {
    $Version += $Matches[1]
}

Write-Output $Version
