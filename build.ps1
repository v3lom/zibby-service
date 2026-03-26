param(
    [switch]$InstallDeps,
    [switch]$TestOnly,
    [string]$BuildType = "Release",
    [bool]$EnableTests = $true,
    [bool]$EnableCalls = $false,
    [bool]$EnablePlugins = $true,
    [bool]$EnablePanel = $true,
    [switch]$Interactive
)

$ErrorActionPreference = "Stop"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
    }
}

function Ensure-MakensisOnPath {
    if (Get-Command makensis -ErrorAction SilentlyContinue) {
        return
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
        "$env:ProgramFiles\NSIS\makensis.exe",
        "$env:SystemDrive\msys64\mingw64\bin\makensis.exe",
        "$env:SystemDrive\tools\msys64\mingw64\bin\makensis.exe"
    )

    foreach ($exe in $candidates) {
        if (Test-Path $exe) {
            $dir = Split-Path -Parent $exe
            if ($env:PATH -notlike "*$dir*") {
                $env:PATH = "$dir;$env:PATH"
            }
            return
        }
    }
}

function Show-Menu {
    Write-Host "Select build type:"
    Write-Host "1) Debug"
    Write-Host "2) Release"
    $choice = Read-Host "Choice"
    if ($choice -eq "1") { return "Debug" }
    return "Release"
}

function Show-YesNo([string]$caption, [bool]$default = $true) {
    $suffix = if ($default) { "[Y/n]" } else { "[y/N]" }
    $value = Read-Host "$caption $suffix"
    if ([string]::IsNullOrWhiteSpace($value)) { return $default }
    return $value.Trim().ToLower() -in @("y", "yes")
}

if ($InstallDeps) {
    & "$PSScriptRoot\scripts\install_deps_windows.ps1"
}

# Auto TUI when script is launched with no explicit parameters from an interactive console.
$autoTui = $false
if (-not $Interactive) {
    if ($PSBoundParameters.Count -eq 0 -and $Host.Name -eq "ConsoleHost") {
        $autoTui = $true
    }
}

if ($Interactive -or $autoTui) {
    # Bootstrap-build and run the interactive TUI builder/installer.
    $bootstrapDir = Join-Path $PSScriptRoot "build\bootstrap"
    if (!(Test-Path $bootstrapDir)) {
        New-Item -ItemType Directory -Path $bootstrapDir | Out-Null
    }

    $cmakeArgs = @(
        "-S", ".",
        "-B", $bootstrapDir,
        "-DZIBBY_ENABLE_TESTS=OFF",
        "-DZIBBY_ENABLE_CALLS=OFF",
        "-DZIBBY_ENABLE_PLUGINS=OFF",
        "-DZIBBY_ENABLE_PANEL=OFF"
    )

    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $cmakeArgs = @("-G", "Ninja") + $cmakeArgs
    }

    # Best-effort vcpkg integration (keeps CI and local setups working).
    if ($env:VCPKG_ROOT) {
        $tc = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $tc) {
            $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$tc"
        }
    } elseif ($env:VCPKG_INSTALLATION_ROOT) {
        $tc = Join-Path $env:VCPKG_INSTALLATION_ROOT "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $tc) {
            $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$tc"
        }
    }

    Invoke-External -FilePath "cmake" -Arguments $cmakeArgs
    Invoke-External -FilePath "cmake" -Arguments @("--build", $bootstrapDir, "--target", "zibby-build-tui")

    $tuiExe = Join-Path $bootstrapDir "zibby-build-tui.exe"
    if (!(Test-Path $tuiExe)) {
        throw "Bootstrap TUI executable not found: $tuiExe"
    }

    & $tuiExe
    exit $LASTEXITCODE
} else {
    $buildType = $BuildType
    $enableTests = $EnableTests
    $enableCalls = $EnableCalls
    $enablePlugins = $EnablePlugins
    $enablePanel = $EnablePanel
}

$buildDir = Join-Path $PSScriptRoot "build"
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Push-Location $buildDir

Write-Host "Configuring with BuildType=$buildType, Tests=$enableTests, Calls=$enableCalls, Plugins=$enablePlugins"

$testsFlag = if ($enableTests) { "ON" } else { "OFF" }
$callsFlag = if ($enableCalls) { "ON" } else { "OFF" }
$pluginsFlag = if ($enablePlugins) { "ON" } else { "OFF" }
$panelFlag = if ($enablePanel) { "ON" } else { "OFF" }

$cmakeConfigureArgs = @(
    "..",
    "-DCMAKE_BUILD_TYPE=$buildType",
    "-DZIBBY_ENABLE_TESTS=$testsFlag",
    "-DZIBBY_ENABLE_CALLS=$callsFlag",
    "-DZIBBY_ENABLE_PLUGINS=$pluginsFlag",
    "-DZIBBY_ENABLE_PANEL=$panelFlag"
)

# Help CMake locate MSYS2-provided packages (Qt6, etc.) in common setups.
$msysMingw64 = Join-Path $env:SystemDrive "msys64\mingw64"
if (Test-Path (Join-Path $msysMingw64 "lib\cmake\Qt6")) {
    if (-not ($cmakeConfigureArgs -match "^-DCMAKE_PREFIX_PATH=")) {
        $cmakeConfigureArgs += "-DCMAKE_PREFIX_PATH=$msysMingw64"
    }
}

# Prefer Ninja if available; otherwise fall back to CMake default generator.
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $cmakeConfigureArgs = @("-G", "Ninja") + $cmakeConfigureArgs
}

Invoke-External -FilePath "cmake" -Arguments $cmakeConfigureArgs

Invoke-External -FilePath "cmake" -Arguments @("--build", ".", "--config", $buildType)

if ($enableTests) {
    Invoke-External -FilePath "ctest" -Arguments @("-C", $buildType, "--output-on-failure")
}

if ($TestOnly) {
    Pop-Location
    exit 0
}

Ensure-MakensisOnPath
Invoke-External -FilePath "cpack" -Arguments @("-C", $buildType)

if (Test-Path (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1")) {
    & (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1") -ArtifactsDir (Join-Path $PSScriptRoot "installers")
}

Pop-Location

