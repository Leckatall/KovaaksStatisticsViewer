$script:KsvToolchainConfigRoot = $PSScriptRoot

function Read-KsvToolchainEnvFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string[]]$AllowedKeys,
        [switch]$Optional
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        if ($Optional) { return @{} }
        throw "Toolchain environment file not found: $Path"
    }

    $values = @{}
    $lineNumber = 0
    foreach ($rawLine in Get-Content -LiteralPath $Path) {
        $lineNumber++
        $line = $rawLine.Trim()
        if (-not $line -or $line.StartsWith("#")) { continue }

        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            throw "Malformed toolchain entry at ${Path}:$lineNumber (expected NAME=value)"
        }

        $key = $line.Substring(0, $separator).Trim()
        $value = $line.Substring($separator + 1).Trim()
        if ($AllowedKeys -notcontains $key) {
            throw "Unknown toolchain key '$key' at ${Path}:$lineNumber"
        }
        if ($values.ContainsKey($key)) {
            throw "Duplicate toolchain key '$key' at ${Path}:$lineNumber"
        }
        if ($value.Length -ge 2) {
            $first = $value.Substring(0, 1)
            $last = $value.Substring($value.Length - 1, 1)
            if (($first -eq '"' -and $last -eq '"') -or ($first -eq "'" -and $last -eq "'")) {
                $value = $value.Substring(1, $value.Length - 2)
            }
        }
        if (-not $value) {
            throw "Empty toolchain value for '$key' at ${Path}:$lineNumber"
        }
        $values[$key] = $value
    }
    return $values
}

function Resolve-KsvToolchainPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$What,
        [ValidateSet("Leaf", "Container")]
        [string]$PathType = "Leaf"
    )

    if (-not [System.IO.Path]::IsPathRooted($Path)) {
        throw "$What must be an absolute path: $Path"
    }
    if (-not (Test-Path -LiteralPath $Path -PathType $PathType)) {
        throw "$What not found at: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-KsvToolchainConfig {
    param([hashtable]$Overrides = @{})

    $keys = @(
        "KSV_CMAKE_EXE",
        "KSV_NINJA_EXE",
        "KSV_C_COMPILER",
        "KSV_CXX_COMPILER",
        "KSV_VCPKG_TOOLCHAIN",
        "KSV_VCPKG_TRIPLET",
        "KSV_VCPKG_BIN_DIR",
        "KSV_PROTOC_EXE",
        "KSV_QT_BIN_DIR",
        "KSV_QT6_DIR"
    )

    $defaultsPath = Join-Path $script:KsvToolchainConfigRoot "toolchain.defaults.env"
    $localPath = Join-Path $script:KsvToolchainConfigRoot "toolchain.local.env"
    $resolved = Read-KsvToolchainEnvFile -Path $defaultsPath -AllowedKeys $keys
    $local = Read-KsvToolchainEnvFile -Path $localPath -AllowedKeys $keys -Optional

    foreach ($key in $keys) {
        if ($local.ContainsKey($key)) { $resolved[$key] = $local[$key] }
        $processValue = [Environment]::GetEnvironmentVariable($key, "Process")
        if ($processValue) { $resolved[$key] = $processValue }
        if ($Overrides.ContainsKey($key) -and $Overrides[$key]) { $resolved[$key] = $Overrides[$key] }
        if (-not $resolved.ContainsKey($key)) { throw "Missing required toolchain key: $key" }
    }

    if (-not $resolved["KSV_VCPKG_TRIPLET"].Trim()) {
        throw "KSV_VCPKG_TRIPLET must not be empty"
    }

    $config = [ordered]@{
        CMakeExe = Resolve-KsvToolchainPath $resolved["KSV_CMAKE_EXE"] "CMake executable"
        NinjaExe = Resolve-KsvToolchainPath $resolved["KSV_NINJA_EXE"] "Ninja executable"
        CCompiler = Resolve-KsvToolchainPath $resolved["KSV_C_COMPILER"] "C compiler"
        CxxCompiler = Resolve-KsvToolchainPath $resolved["KSV_CXX_COMPILER"] "C++ compiler"
        VcpkgToolchain = Resolve-KsvToolchainPath $resolved["KSV_VCPKG_TOOLCHAIN"] "vcpkg toolchain file"
        VcpkgTriplet = $resolved["KSV_VCPKG_TRIPLET"].Trim()
        VcpkgBinDir = Resolve-KsvToolchainPath $resolved["KSV_VCPKG_BIN_DIR"] "vcpkg triplet bin directory" "Container"
        ProtocExe = Resolve-KsvToolchainPath $resolved["KSV_PROTOC_EXE"] "protoc executable"
        QtBinDir = Resolve-KsvToolchainPath $resolved["KSV_QT_BIN_DIR"] "Qt bin directory" "Container"
        Qt6Dir = Resolve-KsvToolchainPath $resolved["KSV_QT6_DIR"] "Qt6 CMake package directory" "Container"
    }

    return [pscustomobject]$config
}
