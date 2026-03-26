$ErrorActionPreference = "Stop"

param(
    # Where to install MSYS2. Defaults to repo-local tools to avoid admin.
    [string]$MsysRoot,
    # Add MSYS2 paths to the current PowerShell process PATH.
    [switch]$AddToProcessPath = $true,
    # Persist MSYS2 paths into the *User* PATH (no admin).
    [switch]$AddToUserPath = $true,
    # Skip interactive prompts.
    [switch]$NoPrompt,
    # Force reinstall / re-download.
    [switch]$Force
)

function Get-RepoRoot {
    $root = Resolve-Path (Join-Path $PSScriptRoot "..")
    return $root.Path
}

function Write-Info([string]$msg) {
    Write-Host "[deps] $msg"
}

function Write-Warn([string]$msg) {
    Write-Host "[deps] WARNING: $msg" -ForegroundColor Yellow
}

function Write-Err([string]$msg) {
    Write-Host "[deps] ERROR: $msg" -ForegroundColor Red
}

function Prompt-YesNo {
    param(
        [Parameter(Mandatory = $true)][string]$Question,
        [bool]$DefaultYes = $true
    )
    if ($NoPrompt) {
        return $DefaultYes
    }
    $suffix = if ($DefaultYes) { "[Y/n]" } else { "[y/N]" }
    $value = Read-Host "$Question $suffix"
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $DefaultYes
    }
    $v = $value.Trim().ToLowerInvariant()
    return $v -in @("y", "yes")
}

function Ensure-Tls {
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    } catch {
        # best-effort
    }
}

function Download-File {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$OutFile
    )

    Ensure-Tls

    $dir = Split-Path -Parent $OutFile
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    Write-Info "Downloading: $Url"
    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing
    } catch {
        throw "Failed to download: $Url"
    }
}

function Prepend-Path([string]$p) {
    if (-not (Test-Path $p)) {
        return
    }
    $parts = $env:PATH -split ';'
    if ($parts -contains $p) {
        return
    }
    $env:PATH = "$p;$env:PATH"
}

function Add-ToUserPath([string[]]$paths) {
    $current = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($null -eq $current) { $current = "" }
    $parts = @()
    if (-not [string]::IsNullOrWhiteSpace($current)) {
        $parts = $current -split ';'
    }
    $changed = $false
    foreach ($p in $paths) {
        if (-not (Test-Path $p)) { continue }
        if ($parts -notcontains $p) {
            $parts += $p
            $changed = $true
        }
    }
    if ($changed) {
        [Environment]::SetEnvironmentVariable("Path", ($parts -join ';'), "User")
        Write-Info "User PATH updated. New terminals will see MSYS2 tools."
    }
}

function Ensure-MsysRoot {
    if ($MsysRoot -and -not [string]::IsNullOrWhiteSpace($MsysRoot)) {
        return
    }

    $repo = Get-RepoRoot
    $MsysRoot = Join-Path $repo "tools\msys64"
}

function Get-BashPath {
    Ensure-MsysRoot
    return (Join-Path $MsysRoot "usr\bin\bash.exe")
}

function Ensure-MSYS2 {
    Ensure-MsysRoot

    $bash = Get-BashPath
    if (-not $Force -and (Test-Path $bash)) {
        Write-Info "MSYS2 already present: $MsysRoot"
        return
    }

    if (Test-Path $MsysRoot) {
        if ($Force) {
            $ok = Prompt-YesNo -Question "Reinstall MSYS2 at $MsysRoot?" -DefaultYes $false
            if ($ok) {
                Remove-Item -Recurse -Force $MsysRoot -ErrorAction SilentlyContinue
            }
        }
    }

    New-Item -ItemType Directory -Path $MsysRoot -Force | Out-Null

    $url = "https://github.com/msys2/msys2-installer/releases/latest/download/msys2-base-x86_64-latest.sfx.exe"
    $tmp = Join-Path $env:TEMP "zibby-msys2-base-x86_64-latest.sfx.exe"
    Download-File -Url $url -OutFile $tmp

    Write-Info "Extracting MSYS2 to $MsysRoot (may take a minute)..."
    $p = Start-Process -FilePath $tmp -ArgumentList @("-y", "-o$MsysRoot") -Wait -PassThru
    if ($p.ExitCode -ne 0) {
        throw "MSYS2 extraction failed (exit $($p.ExitCode))"
    }
    if (-not (Test-Path $bash)) {
        throw "MSYS2 extraction did not produce bash.exe; install is incomplete: $MsysRoot"
    }

    Write-Info "MSYS2 ready: $MsysRoot"
}

function Invoke-MsysBash {
    param(
        [Parameter(Mandatory = $true)][string]$Command
    )

    $bash = Get-BashPath
    if (-not (Test-Path $bash)) {
        throw "bash.exe not found: $bash"
    }

    # Use -l (login) to make MSYS2 environment consistent.
    & $bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed ($LASTEXITCODE): $Command"
    }
}

function Ensure-PacmanReady {
    Write-Info "Initializing pacman (keyring + sync)..."
    try {
        Invoke-MsysBash "pacman-key --init" | Out-Host
    } catch {
        # best-effort
    }
    try {
        Invoke-MsysBash "pacman-key --populate msys2" | Out-Host
    } catch {
        # best-effort
    }

    # First sync/update (best-effort). Updates sometimes require two passes.
    try {
        Invoke-MsysBash "pacman -Syuu --noconfirm" | Out-Host
    } catch {
        Write-Warn "Initial pacman full update failed; continuing with install (often ok on fresh setups)."
    }
    try {
        Invoke-MsysBash "pacman -Sy --noconfirm" | Out-Host
    } catch {
        # required
        throw
    }
}

function Install-MSYS2Packages {
    param(
        [Parameter(Mandatory = $true)][string[]]$Packages
    )

    $pkgList = ($Packages | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join " "
    Write-Info "Installing MSYS2 packages: $pkgList"
    Invoke-MsysBash "pacman -S --needed --noconfirm $pkgList" | Out-Host
}

function Ensure-Paths {
    Ensure-MsysRoot

    $usrBin = Join-Path $MsysRoot "usr\bin"
    $mingwBin = Join-Path $MsysRoot "mingw64\bin"

    if ($AddToProcessPath) {
        Prepend-Path $mingwBin
        Prepend-Path $usrBin
        Write-Info "Process PATH updated (for this terminal)."
    }

    if ($AddToUserPath) {
        $ok = Prompt-YesNo -Question "Add MSYS2 paths to USER PATH (recommended)?" -DefaultYes $true
        if ($ok) {
            Add-ToUserPath @($mingwBin, $usrBin)
        }
    }
}

try {
    Ensure-MSYS2
    Ensure-PacmanReady

    # Core toolchain + build tools + libs.
    # Note: we install cmake/git/ninja *from MSYS2* to avoid relying on winget/choco.
    Install-MSYS2Packages -Packages @(
        "base-devel",
        "git",
        "mingw-w64-x86_64-toolchain",
        "mingw-w64-x86_64-cmake",
        "mingw-w64-x86_64-ninja",
        "mingw-w64-x86_64-boost",
        "mingw-w64-x86_64-openssl",
        "mingw-w64-x86_64-sqlite3",
        "mingw-w64-x86_64-nsis",
        "mingw-w64-x86_64-qt6-base"
    )

    Ensure-Paths

    # Quick sanity checks
    Write-Info "Sanity check: cmake/git/g++"
    Invoke-MsysBash "cmake --version | head -n 1"
    Invoke-MsysBash "git --version"
    Invoke-MsysBash "g++ --version | head -n 1"

    Write-Info "Done. If you opened a new terminal, PATH changes will apply there too."
    exit 0
} catch {
    Write-Err $_
    exit 1
}
