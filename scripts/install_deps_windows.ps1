$ErrorActionPreference = "Stop"

function Install-MSYS2-IfMissing {
    param(
        [string]$PreferredRoot = "$env:SystemDrive\msys64"
    )

    if (Test-Path (Join-Path $PreferredRoot "usr\bin\pacman.exe")) {
        return
    }

    $fallbackRoot = Join-Path $env:USERPROFILE "msys64"
    if (Test-Path (Join-Path $fallbackRoot "usr\bin\pacman.exe")) {
        return
    }

    $url = "https://github.com/msys2/msys2-installer/releases/latest/download/msys2-base-x86_64-latest.sfx.exe"
    $tmp = Join-Path $env:TEMP "msys2-base-x86_64-latest.sfx.exe"

    Write-Host "MSYS2 not found; downloading bootstrap installer..."
    try {
        Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing
    } catch {
        Write-Host "Failed to download MSYS2 from $url"
        Write-Host "Install MSYS2 manually from https://www.msys2.org/ then re-run this script."
        return
    }

    # Prefer system drive, but fall back to user profile if not permitted.
    $targetParent = "$env:SystemDrive\"
    try {
        New-Item -ItemType Directory -Path $PreferredRoot -Force | Out-Null
        $targetParent = "$env:SystemDrive\"
    } catch {
        Write-Host "No permission to write to $PreferredRoot; using $fallbackRoot"
        New-Item -ItemType Directory -Path $fallbackRoot -Force | Out-Null
        $targetParent = (Split-Path -Parent $fallbackRoot) + "\"
    }

    Write-Host "Extracting MSYS2 to $targetParent (this may take a minute)..."
    & $tmp -y -o$targetParent
    if ($LASTEXITCODE -ne 0) {
        Write-Host "MSYS2 extraction failed (exit $LASTEXITCODE)"
        return
    }

    Write-Host "MSYS2 extracted. You may need to open a new terminal for PATH changes to apply."
}

function Install-NSIS-WithWinget {
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        # NSIS provides makensis.exe (used by CPack NSIS generator)
        winget install --id NSIS.NSIS -e --accept-package-agreements --accept-source-agreements
    }
}

function Install-NSIS-WithChoco {
    if (Get-Command choco -ErrorAction SilentlyContinue) {
        choco install -y nsis
    }
}

function Find-Pacman {
    if (Get-Command pacman -ErrorAction SilentlyContinue) {
        return (Get-Command pacman).Source
    }

    $known = @(
        "$env:SystemDrive\msys64\usr\bin\pacman.exe",
        "$env:SystemDrive\tools\msys64\usr\bin\pacman.exe"
    )
    foreach ($p in $known) {
        if (Test-Path $p) { return $p }
    }

    return $null
}

function Install-MSYS2Deps {
    Install-MSYS2-IfMissing
    $pacman = Find-Pacman
    if (-not $pacman) {
        Write-Host "MSYS2 pacman not found; skipping MSYS2 packages."
        return
    }

    Write-Host "Installing MSYS2 packages via pacman: toolchain, boost, openssl, sqlite3, ninja, nsis, qt6"
    & $pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-nsis mingw-w64-x86_64-qt6-base
    if ($LASTEXITCODE -ne 0) {
        throw "pacman failed with exit code $LASTEXITCODE"
    }
}

function Use-Winget {
    winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
    winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements
    winget install --id LLVM.LLVM -e --accept-package-agreements --accept-source-agreements
    Install-NSIS-WithWinget
}

function Use-Choco {
    choco install -y cmake git llvm
    Install-NSIS-WithChoco
}

if (Get-Command winget -ErrorAction SilentlyContinue) {
    Use-Winget
} elseif (Get-Command choco -ErrorAction SilentlyContinue) {
    Use-Choco
} else {
    Write-Host "Neither winget nor choco found. Install dependencies manually."
    exit 1
}

Install-MSYS2Deps

Write-Host "Done. If you're using MSYS2 MinGW64 toolchain, make sure these are on PATH when building:"
Write-Host "  C:\\msys64\\mingw64\\bin"
Write-Host "  C:\\msys64\\usr\\bin"
