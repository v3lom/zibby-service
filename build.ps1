param(
    [switch]$InstallDeps,
    [switch]$TestOnly,
    [switch]$BootstrapTuiOnly,
    [string]$BuildType = "Release",
    [bool]$EnableTests = $true,
    [bool]$EnableCalls = $false,
    [bool]$EnablePlugins = $true,
    [bool]$EnablePanel = $true,
    [switch]$Interactive
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return (Resolve-Path $PSScriptRoot).Path
}

function Add-MsysToPathIfPresent {
    $repo = Get-RepoRoot
    $candidates = @(
        (Join-Path $repo "tools\msys64"),
        "$env:SystemDrive\msys64",
        (Join-Path $env:USERPROFILE "msys64"),
        "$env:SystemDrive\tools\msys64"
    )

    foreach ($root in $candidates) {
        $usr = Join-Path $root "usr\bin"
        $mingw = Join-Path $root "mingw64\bin"
        if (Test-Path (Join-Path $usr "bash.exe")) {
            if ($env:PATH -notlike "*$mingw*") { $env:PATH = "$mingw;$env:PATH" }
            if ($env:PATH -notlike "*$usr*") { $env:PATH = "$usr;$env:PATH" }
            return $root
        }
    }
    return $null
}

function Test-IsAdmin {
    try {
        $id = [Security.Principal.WindowsIdentity]::GetCurrent()
        $p = New-Object Security.Principal.WindowsPrincipal($id)
        return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    } catch {
        return $false
    }
}

function Prompt-YesNo {
    param(
        [Parameter(Mandatory = $true)][string]$Question,
        [bool]$DefaultYes = $true
    )
    $suffix = if ($DefaultYes) { "[Y/n]" } else { "[y/N]" }
    $value = Read-Host "$Question $suffix"
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $DefaultYes
    }
    $v = $value.Trim().ToLowerInvariant()
    return $v -in @("y", "yes")
}

function Invoke-ElevatedPowerShell {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [string[]]$ScriptArgs = @()
    )

    $argList = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $ScriptPath
    ) + $ScriptArgs

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "powershell.exe"
    $psi.Arguments = ($argList | ForEach-Object {
        if ($_ -match "\s") { '"' + ($_ -replace '"','\\"') + '"' } else { $_ }
    }) -join " "
    $psi.Verb = "runas"
    $psi.UseShellExecute = $true

    $p = [System.Diagnostics.Process]::Start($psi)
    $p.WaitForExit()
    return $p.ExitCode
}

function Ensure-WindowsPrereqs {
    # Minimal prerequisites to run CMake bootstrap and full build.
    # First, try to pick up MSYS2-provided tools if MSYS2 is already installed.
    [void](Add-MsysToPathIfPresent)

    $missing = @()
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $missing += "cmake" }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { $missing += "git" }

    if ($missing.Count -eq 0) {
        return
    }

    Write-Host "Missing tools: $($missing -join ', ')"
    $ok = Prompt-YesNo -Question "Install required dependencies automatically? (downloads MSYS2 + packages; no winget/choco)" -DefaultYes $true
    if (-not $ok) {
        throw "Required tools are missing: $($missing -join ', ')"
    }

    $depsScript = Join-Path $PSScriptRoot "scripts\install_deps_windows.ps1"
    if (!(Test-Path $depsScript)) {
        throw "Dependency installer not found: $depsScript"
    }

    # Prefer per-user / repo-local install to avoid requiring admin.
    & $depsScript
    if ($LASTEXITCODE -ne 0) {
        throw "Dependency installer failed ($LASTEXITCODE)"
    }

    # Re-check after installation.
    [void](Add-MsysToPathIfPresent)
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw "cmake is still not available after install" }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "git is still not available after install" }
}

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
    $depsScript = Join-Path $PSScriptRoot "scripts\install_deps_windows.ps1"
    $ok = Prompt-YesNo -Question "Install dependencies now?" -DefaultYes $true
    if (-not $ok) {
        exit 2
    }
    & $depsScript
    exit $LASTEXITCODE
}

# Auto TUI when script is launched with no explicit parameters from an interactive console.
$autoTui = $false
if (-not $Interactive) {
    if ($PSBoundParameters.Count -eq 0 -and $Host.Name -eq "ConsoleHost") {
        $autoTui = $true
    }
}

if ($Interactive -or $autoTui) {
    Ensure-WindowsPrereqs
    # Bootstrap-build and run the interactive TUI builder/installer.
    $bootstrapDir = Join-Path $PSScriptRoot "build\bootstrap"
    if (!(Test-Path $bootstrapDir)) {
        New-Item -ItemType Directory -Path $bootstrapDir | Out-Null
    }

    $cmakeArgs = @(
        "-S", ".",
        "-B", $bootstrapDir,
        "-DZIBBY_BOOTSTRAP_TUI_ONLY=ON",
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

    # Convenience: keep a copy in repo root so it can be started without .bat/.ps1.
    $rootExe = Join-Path $PSScriptRoot "zibby-build-tui.exe"
    Copy-Item -Force $tuiExe $rootExe

    & $tuiExe
    exit $LASTEXITCODE
} elseif ($BootstrapTuiOnly) {
    Ensure-WindowsPrereqs
    $bootstrapDir = Join-Path $PSScriptRoot "build\bootstrap"
    if (!(Test-Path $bootstrapDir)) {
        New-Item -ItemType Directory -Path $bootstrapDir | Out-Null
    }

    $cmakeArgs = @(
        "-S", ".",
        "-B", $bootstrapDir,
        "-DZIBBY_BOOTSTRAP_TUI_ONLY=ON",
        "-DZIBBY_ENABLE_TESTS=OFF",
        "-DZIBBY_ENABLE_CALLS=OFF",
        "-DZIBBY_ENABLE_PLUGINS=OFF",
        "-DZIBBY_ENABLE_PANEL=OFF"
    )

    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $cmakeArgs = @("-G", "Ninja") + $cmakeArgs
    }

    Invoke-External -FilePath "cmake" -Arguments $cmakeArgs
    Invoke-External -FilePath "cmake" -Arguments @("--build", $bootstrapDir, "--target", "zibby-build-tui")

    $tuiExe = Join-Path $bootstrapDir "zibby-build-tui.exe"
    if (!(Test-Path $tuiExe)) {
        throw "Bootstrap TUI executable not found: $tuiExe"
    }

    $rootExe = Join-Path $PSScriptRoot "zibby-build-tui.exe"
    Copy-Item -Force $tuiExe $rootExe

    Write-Host "Built: $rootExe"
    exit 0
} else {
    Ensure-WindowsPrereqs
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

