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
    4. Smoke-tests the packaged exe under a scrubbed PATH.
    5. Zips the result as KovaaksStatsViewer-<Version>-win64.zip.

    The version comes from CMakeLists.txt via scripts/version.ps1; -Version
    overrides it.

    The build directory is deleted before configuring unless -Incremental is
    passed, so a release is always built from scratch - see the comment above
    the build section for why.

    Tool paths below default to this machine's current setup (CLion's
    bundled MinGW/Ninja + the vcpkg instance CLion manages). Override
    via parameters if your environment differs, e.g. when running this
    from CI.

.EXAMPLE
    ./scripts/package-release.ps1
    ./scripts/package-release.ps1 -Version 0.1.0-alpha
    ./scripts/package-release.ps1 -ValidateOnly
    ./scripts/package-release.ps1 -Incremental
#>

[CmdletBinding()]
param(
    [string]$Version,

    [string]$RepoRoot,
    [string]$BuildDir,
    [string]$DistDir,

    [string]$VcpkgToolchain = "C:/Users/Lecka/.vcpkg-clion/vcpkg (1)/scripts/buildsystems/vcpkg.cmake",
    [string]$VcpkgTriplet   = "x64-mingw-dynamic",
    [string]$VcpkgBinDir    = "C:/Users/Lecka/.vcpkg-clion/vcpkg (1)/installed/x64-mingw-dynamic/bin",

    [string]$MingwBinDir = "C:/Program Files/JetBrains/CLion 2024.1.1/bin/mingw/bin",
    [string]$NinjaExe    = "C:/Program Files/JetBrains/CLion 2024.1.1/bin/ninja/win/x64/ninja.exe",

    [string]$QtBinDir = "C:/Qt/6.11.1/mingw_64/bin",

    [switch]$SkipBuild,
    [switch]$Incremental,
    [switch]$ValidateOnly,
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"

# Keep the scripts in this directory ASCII-only. Windows PowerShell 5.1 (the only PowerShell on this
# machine) reads a .ps1 with no BOM as ANSI, so a UTF-8 em dash decodes to a curly quote, which 5.1
# accepts as a string delimiter: the literal ends early and the parse errors surface dozens of lines
# away from the character that caused them.

# `throw` is how every failure below is signalled, and the exit code is what release automation reads.
# An uncaught throw already exits 1 under `powershell -File`, but that is load-bearing enough to make
# explicit rather than inherit.
trap { Write-Error $_; exit 1 }

function Assert-PathExists([string]$Path, [string]$What) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$What not found at: $Path (pass the matching -parameter to override)"
    }
}

if ($SkipBuild -and $Incremental) {
    throw "-SkipBuild and -Incremental are contradictory: -SkipBuild packages the existing build without touching it, -Incremental rebuilds into it."
}

# These are resolved here rather than as param defaults because `powershell -File` evaluates param
# defaults before $PSScriptRoot is populated: "$PSScriptRoot/.." would quietly resolve to the root of
# the current drive, and the whole build and package would land in C:\build-release and C:\dist.
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path "$PSScriptRoot/..").Path }
if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build-release" }

if (-not $Version) {
    $Version = & (Join-Path $PSScriptRoot "version.ps1") -RepoRoot $RepoRoot
}
if (-not $DistDir) {
    $DistDir = "$RepoRoot/dist/KovaaksStatsViewer-$Version-win64"
}
$ZipPath = "$RepoRoot/dist/KovaaksStatsViewer-$Version-win64.zip"

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

Assert-PathExists (Join-Path $QtBinDir "windeployqt.exe") "windeployqt"
Assert-PathExists $VcpkgBinDir "vcpkg triplet bin dir"

if ($ValidateOnly) {
    Write-Host "`n== Environment OK ==" -ForegroundColor Green
    Write-Host "All tool paths resolved; nothing was built or packaged."
    exit 0
}

if (-not $SkipBuild) {
    # A stale build directory can produce a package whose app is silently out of date, and the build
    # still reports success. Two ways it happens: CMake honours CMAKE_TOOLCHAIN_FILE, the compiler and
    # the generator only on the *first* configure, so re-running against a directory first configured
    # differently builds the old configuration while printing the new flags; and the Qt codegen
    # (AUTOMOC/AUTORCC/qmlcachegen) output lives here, so a source timestamp that fails to advance past
    # its generated artifact leaves the previous QML baked into the binary. Neither is visible in an
    # exit code. A release has to be reproducible from nothing, so the default is to start from
    # nothing; -Incremental trades that guarantee for build time during packaging experiments.
    if ($Incremental) {
        Write-Host "`n-- Incremental build: reusing $BuildDir (not release-safe) --" -ForegroundColor Yellow
    }
    elseif (Test-Path -LiteralPath $BuildDir) {
        Write-Host "`n-- Removing existing $BuildDir for a clean configure --" -ForegroundColor Cyan
        Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }

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

    # CMake only warns when a cached toolchain/compiler/build type disagrees with the flags it was just
    # given, so read back what the cache actually holds. Redundant after a clean configure; this is the
    # check that catches -Incremental pointed at a directory configured by something else.
    $CachePath = Join-Path $BuildDir "CMakeCache.txt"
    Assert-PathExists $CachePath "CMakeCache.txt"
    $Cache = Get-Content -Raw -LiteralPath $CachePath
    foreach ($expected in @(
        @{ Key = "CMAKE_BUILD_TYPE";      Value = "Release" },
        @{ Key = "VCPKG_TARGET_TRIPLET";  Value = $VcpkgTriplet },
        @{ Key = "CMAKE_CXX_COMPILER";    Value = $CxxCompiler }
    )) {
        $pattern = "(?m)^$([regex]::Escape($expected.Key)):[^=]*=(.*)$"
        if ($Cache -notmatch $pattern) {
            throw "$($expected.Key) missing from $CachePath"
        }
        $actual = $Matches[1].Trim()
        if ($actual.Replace("\", "/") -ne $expected.Value.Replace("\", "/")) {
            throw "$($expected.Key) in the CMake cache is '$actual', expected '$($expected.Value)'. The build directory was configured with different settings; drop -Incremental to reconfigure from scratch."
        }
    }

    Write-Host "`n-- Building --" -ForegroundColor Cyan
    cmake --build $BuildDir
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
$ExeName = "KovaaksStatsViewer.exe"
$ExePath = Join-Path $DistDir $ExeName
Copy-Item $ExeSource -Destination $ExePath

Write-Host "`n-- Running windeployqt --" -ForegroundColor Cyan
$WinDeployQt = Join-Path $QtBinDir "windeployqt.exe"

& $WinDeployQt `
    --release `
    --compiler-runtime `
    --qmldir "$RepoRoot/src/ui/qml" `
    $ExePath
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

Write-Host "`n-- Copying vcpkg runtime DLLs ($VcpkgTriplet) --" -ForegroundColor Cyan
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
    # Fatal, not a warning: this is the exact condition the comment above describes as breaking
    # startup on a clean PC, and a release that ships without it looks successful until a user runs it.
    Assert-PathExists $src "MinGW runtime DLL $dll"
    Copy-Item $src -Destination $DistDir -Force
    Write-Host "  forced $dll from $MingwBinDir"
}

if (-not $SkipSmokeTest) {
    Write-Host "`n-- Smoke-testing the packaged exe --" -ForegroundColor Cyan
    # Launch what was just packaged with the toolchain directories stripped from PATH, so the DLLs in
    # dist/ are the only ones it can resolve: precisely the situation on a user's machine, and the
    # failure this whole DLL-copying dance exists to prevent. The exe's imports are resolved by the
    # loader before any platform plugin is chosen, so running headless tests exactly the same thing as
    # running windowed. Not a functional test: nothing is rendered or clicked.
    #
    # The pass condition is the module list, NOT survival. When a required DLL is missing the loader
    # raises a hard error, and Windows keeps the doomed process alive behind a "code execution cannot
    # proceed" message box until someone dismisses it - so a process that never loaded Qt at all looks
    # identical to a healthy one if you only ask whether it is still running. Verified: deleting
    # Qt6Core.dll from dist/ leaves a process that is still "alive" after 5s with 34 modules, against
    # roughly 90 for a healthy start.
    #
    # windeployqt ships only qwindows.dll, so the offscreen plugin has to be borrowed from the Qt
    # install for the duration of the test and removed again - without it Qt aborts on startup
    # ("no Qt platform plugin could be initialized") and the smoke test reports a false failure.
    $OffscreenSource = Join-Path (Split-Path $QtBinDir -Parent) "plugins/platforms/qoffscreen.dll"
    $OffscreenTemp   = Join-Path $DistDir "platforms/qoffscreen.dll"
    if (-not (Test-Path -LiteralPath $OffscreenSource)) {
        Write-Warning "  qoffscreen.dll not found at $OffscreenSource - skipping smoke test"
    }
    else {
        # Loading these proves the whole dependency chain resolved out of dist/ alone: the Qt stack up
        # through QML, the vcpkg libs windeployqt knows nothing about, and the MinGW runtime forced in
        # above. All are present in a healthy start.
        $RequiredModules = @(
            "Qt6Core.dll", "Qt6Gui.dll", "Qt6Qml.dll", "Qt6Quick.dll",
            "libprotobuf.dll", "libabseil_dll.dll",
            "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"
        )

        $SavedPath     = $env:PATH
        $SavedPlatform = $env:QT_QPA_PLATFORM
        Copy-Item $OffscreenSource -Destination $OffscreenTemp -Force
        try {
            $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"
            $env:QT_QPA_PLATFORM = "offscreen"
            $proc = Start-Process -FilePath $ExePath -WorkingDirectory $DistDir -PassThru
            Start-Sleep -Seconds 5
            $proc.Refresh()

            if ($proc.HasExited) {
                throw "Packaged exe exited within 5s with code $($proc.ExitCode) (0x$('{0:X8}' -f $proc.ExitCode)). Codes like 0xC0000139 (ENTRY_POINT_NOT_FOUND) or 0xC0000135 (DLL_NOT_FOUND) mean a runtime DLL is missing or mismatched in dist/. Re-run with -SkipSmokeTest to package anyway."
            }

            $Loaded  = $proc.Modules | ForEach-Object { $_.ModuleName }
            $Missing = $RequiredModules | Where-Object { $Loaded -notcontains $_ }
            if ($Missing) {
                throw "Packaged exe started but never loaded: $($Missing -join ', '). It is almost certainly stuck on a loader error dialog because a DLL is missing from dist/ ($($Loaded.Count) modules loaded). Re-run with -SkipSmokeTest to package anyway."
            }

            Write-Host "  loaded $($Loaded.Count) modules under a scrubbed PATH, including the full Qt/vcpkg/MinGW chain"
        }
        finally {
            $env:PATH = $SavedPath
            $env:QT_QPA_PLATFORM = $SavedPlatform
            # Kill unconditionally: on the failure paths above the process is still sitting on a modal
            # loader error dialog, and nothing else will ever close it.
            if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
            # The borrowed plugin must not reach the zip. Windows holds the file open until the process
            # has fully exited, so killing it is not by itself enough for the delete to succeed.
            if ($proc) { $proc.WaitForExit(10000) | Out-Null }
            Remove-Item -LiteralPath $OffscreenTemp -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $OffscreenTemp) {
                throw "Could not remove the temporary $OffscreenTemp; delete it before zipping or the package will ship it."
            }
        }
    }
}

Write-Host "`n-- Zipping --" -ForegroundColor Cyan
if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}
# Entries are added one at a time so their names can be normalised. On .NET Framework - which is what
# Windows PowerShell 5.1 runs on - both Compress-Archive and ZipFile.CreateFromDirectory write the
# platform separator into entry names, and the ZIP spec requires '/'. Windows tools tolerate the
# backslashes, but unzip on Linux/macOS treats "platforms\qwindows.dll" as a single flat filename.
# Every name is also prefixed with the dist folder, so the archive expands into one directory instead
# of scattering ~1500 files into whatever directory the user extracted it to.
Add-Type -AssemblyName System.IO.Compression          # ZipArchive, ZipArchiveMode, CompressionLevel
Add-Type -AssemblyName System.IO.Compression.FileSystem  # ZipFile, ZipFileExtensions
$DistRoot   = (Resolve-Path -LiteralPath $DistDir).Path.TrimEnd('\')
$ArchiveTop = Split-Path $DistRoot -Leaf
$Archive    = [System.IO.Compression.ZipFile]::Open($ZipPath, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $DistRoot -Recurse -File) {
        $relative = $file.FullName.Substring($DistRoot.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $Archive, $file.FullName, "$ArchiveTop/$relative",
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}
finally {
    $Archive.Dispose()
}

Write-Host "`n== Done ==" -ForegroundColor Green
Write-Host "Folder : $DistDir"
Write-Host "Zip    : $ZipPath"
Write-Host "`nThe smoke test only proves it starts. Before publishing, test this folder on a machine/account without Qt, vcpkg, or MinGW on PATH." -ForegroundColor Yellow
