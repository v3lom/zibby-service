param(
    [switch]$InstallDeps,
    [switch]$TestOnly,
    [string]$BuildType = "Release",
    [bool]$EnableTests = $true,
    [bool]$EnableCalls = $false,
    [bool]$EnablePlugins = $true,
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

if ($Interactive) {
    $buildType = Show-Menu
    $enableTests = Show-YesNo "Enable tests?"
    $enableCalls = Show-YesNo "Enable calls module?" $false
    $enablePlugins = Show-YesNo "Enable plugins module?"
} else {
    $buildType = $BuildType
    $enableTests = $EnableTests
    $enableCalls = $EnableCalls
    $enablePlugins = $EnablePlugins
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

$cmakeConfigureArgs = @(
    "..",
    "-DCMAKE_BUILD_TYPE=$buildType",
    "-DZIBBY_ENABLE_TESTS=$testsFlag",
    "-DZIBBY_ENABLE_CALLS=$callsFlag",
    "-DZIBBY_ENABLE_PLUGINS=$pluginsFlag"
)

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

Invoke-External -FilePath "cpack" -Arguments @("-C", $buildType)

if (Test-Path (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1")) {
    & (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1") -ArtifactsDir (Join-Path $PSScriptRoot "installers")
}

Pop-Location

