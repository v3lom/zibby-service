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

cmake .. `
    -G "Ninja" `
    -DCMAKE_BUILD_TYPE=$buildType `
    -DBUILD_TESTS=$(if ($enableTests) {"ON"} else {"OFF"}) `
    -DBUILD_CALLS=$(if ($enableCalls) {"ON"} else {"OFF"}) `
    -DBUILD_PLUGINS=$(if ($enablePlugins) {"ON"} else {"OFF"})

if ($TestOnly) {
    cmake --build . --config $buildType --target zibby-service-tests
    ctest -C $buildType --output-on-failure
    Pop-Location
    exit 0
}

cmake --build . --config $buildType
cpack -C $buildType

if (Test-Path (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1")) {
    & (Join-Path $PSScriptRoot "scripts\generate_checksums.ps1") -ArtifactsDir (Join-Path $PSScriptRoot "installers")
}

Pop-Location

