<#
.SYNOPSIS
    Builds KovaaksStatsViewer in Release mode and packages it into a
    self-contained, zipped folder that can run on a PC without Qt,
    vcpkg, or MinGW installed.

.DESCRIPTION
    1. Configures + builds a Release build with the same vcpkg toolchain/
       triplet (x64-mingw-dynamic) the CLion Debug profile uses.
    2. Runs windeployqt to pull in Qt DLLs, platform plugins, and the
       compiled QML module.
    3. Copies the vcpkg runtime DLLs (protobuf, abseil, ...) and the
       MinGW runtime DLLs, since windeployqt only knows about Qt's own
       dependencies.
    4. Zips the result as KovaaksStatsViewer-<Version>-win64.zip.

    Tool paths below default to this machine's current setup (CLion's
    bundled MinGW/Ninja + the vcpkg instance CLion manages). Override
    via parameters if your environment differs, e.g. when running this
    from CI.

.EXAMPLE
    ./scripts/package-release.ps1
    ./scripts/package-release.ps1 -Version 0.1.0-alpha
#>

[CmdletBinding()]
param(
    [string]$Version = "0.2.0-alpha",

    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot/.."),
    [string]$BuildDir = "$RepoRoot/build-release",
    [string]$DistDir  = "$RepoRoot/dist/KovaaksStatsViewer-$Version-win64",

    [string]$VcpkgToolchain = "C:/Users/Lecka/.vcpkg-clion/vcpkg (1)/scripts/buildsystems/vcpkg.cmake",
    [string]$VcpkgTriplet   = "x64-mingw-dynamic",
    [string]$VcpkgBinDir    = "C:/Users/Lecka/.vcpkg-clion/vcpkg (1)/installed/x64-mingw-dynamic/bin",

    [string]$MingwBinDir = "C:/Program Files/JetBrains/CLion 2024.1.1/bin/mingw/bin",
    [string]$NinjaExe    = "C:/Program Files/JetBrains/CLion 2024.1.1/bin/ninja/win/x64/ninja.exe",

    [string]$QtBinDir = "C:/Qt/6.11.1/mingw_64/bin",

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Assert-PathExists([string]$Path, [string]$What) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$What not found at: $Path (pass the matching -parameter to override)"
    }
}

Write-Host "== KovaaksStatsViewer release packager ==" -ForegroundColor Cyan
Write-Host "Version : $Version"
Write-Host "Repo    : $RepoRoot"
Write-Host "Dist    : $DistDir"

Assert-PathExists $VcpkgToolchain "vcpkg toolchain file"
Assert-PathExists $MingwBinDir    "MinGW bin dir"
Assert-PathExists $NinjaExe       "Ninja executable"
Assert-PathExists $QtBinDir       "Qt bin dir"

$CxxCompiler = Join-Path $MingwBinDir "c++.exe"
$CCompiler   = Join-Path $MingwBinDir "gcc.exe"
Assert-PathExists $CxxCompiler "MinGW c++ compiler"
Assert-PathExists $CCompiler   "MinGW gcc compiler"

if (-not $SkipBuild) {
    Write-Host "`n-- Configuring (Release) --" -ForegroundColor Cyan
    cmake -B $BuildDir -S $RepoRoot -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_MAKE_PROGRAM="$NinjaExe" `
        -DCMAKE_CXX_COMPILER="$CxxCompiler" `
        -DCMAKE_C_COMPILER="$CCompiler" `
        -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain" `
        -DVCPKG_TARGET_TRIPLET="$VcpkgTriplet" `
        -DBUILD_TESTING=OFF
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

    Write-Host "`n-- Building --" -ForegroundColor Cyan
    cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
} else {
    Write-Host "`n-- Skipping build (using existing $BuildDir) --" -ForegroundColor Yellow
}

$ExeSource = Join-Path $BuildDir "ksv.exe"
Assert-PathExists $ExeSource "Built executable"

Write-Host "`n-- Assembling dist folder --" -ForegroundColor Cyan
if (Test-Path -LiteralPath $DistDir) {
    Remove-Item -LiteralPath $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path $DistDir | Out-Null
Copy-Item $ExeSource -Destination (Join-Path $DistDir "KovaaksStatsViewer.exe")

Write-Host "`n-- Running windeployqt --" -ForegroundColor Cyan
$WinDeployQt = Join-Path $QtBinDir "windeployqt.exe"
Assert-PathExists $WinDeployQt "windeployqt"

& $WinDeployQt `
    --release `
    --compiler-runtime `
    --qmldir "$RepoRoot/src/ui/qml" `
    (Join-Path $DistDir "KovaaksStatsViewer.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

Write-Host "`n-- Copying vcpkg runtime DLLs ($VcpkgTriplet) --" -ForegroundColor Cyan
Assert-PathExists $VcpkgBinDir "vcpkg triplet bin dir"
# windeployqt only knows about Qt's own dependency graph, so the vcpkg-provided
# runtime libs (protobuf, abseil, ...) have to be copied in separately. Copying
# the whole triplet bin/ is deliberately over-inclusive rather than hand-listing
# DLL names that will silently drift as dependencies change; trim it later with
# a dependency walker (e.g. `ntldd -R`) if a smaller package matters.
Copy-Item "$VcpkgBinDir/*.dll" -Destination $DistDir -Force

Write-Host "`n-- Forcing MinGW runtime DLLs to match the compiler that built the app --" -ForegroundColor Cyan
# windeployqt's --compiler-runtime pulls its own mingw runtime (from Qt's mingw
# build) and the vcpkg triplet bin/ may ship yet another copy. Both can
# overwrite libwinpthread-1.dll with a version that's missing symbols abseil
# needs (e.g. pthread_cond_timedwait64), which surfaces as "The procedure
# entry point ... could not be located" at launch. The only copy proven to
# work is the one next to the compiler that actually built myapp.exe/abseil
# (see the matching comment in the top-level CMakeLists.txt), so copy those
# in last and unconditionally overwrite whatever's already in dist/.
foreach ($dll in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
    $src = Join-Path $MingwBinDir $dll
    if (Test-Path -LiteralPath $src) {
        Copy-Item $src -Destination $DistDir -Force
        Write-Host "  forced $dll from $MingwBinDir"
    } else {
        Write-Warning "  $dll not found in $MingwBinDir - build may fail to launch on a clean PC"
    }
}

$ZipPath = "$RepoRoot/dist/KovaaksStatsViewer-$Version-win64.zip"
Write-Host "`n-- Zipping --" -ForegroundColor Cyan
if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -Path "$DistDir/*" -DestinationPath $ZipPath

Write-Host "`n== Done ==" -ForegroundColor Green
Write-Host "Folder : $DistDir"
Write-Host "Zip    : $ZipPath"
Write-Host "`nBefore publishing: test this folder on a machine/account without Qt, vcpkg, or MinGW on PATH." -ForegroundColor Yellow
