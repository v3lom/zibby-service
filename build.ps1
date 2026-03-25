param(
    [switch]$InstallDeps
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

$buildType = Show-Menu
$enableTests = Show-YesNo "Enable tests?"
$enableCalls = Show-YesNo "Enable calls module?" $false
$enablePlugins = Show-YesNo "Enable plugins module?"

$buildDir = Join-Path $PSScriptRoot "build"
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Push-Location $buildDir

cmake .. `
    -DCMAKE_BUILD_TYPE=$buildType `
    -DZIBBY_ENABLE_TESTS=$(if ($enableTests) {"ON"} else {"OFF"}) `
    -DZIBBY_ENABLE_CALLS=$(if ($enableCalls) {"ON"} else {"OFF"}) `
    -DZIBBY_ENABLE_PLUGINS=$(if ($enablePlugins) {"ON"} else {"OFF"})

cmake --build . --config $buildType
ctest -C $buildType --output-on-failure
cpack -C $buildType

Pop-Location
