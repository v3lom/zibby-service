$ErrorActionPreference = "Stop"

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
    $pacman = Find-Pacman
    if (-not $pacman) {
        Write-Host "MSYS2 pacman not found; skipping MSYS2 packages."
        return
    }

    Write-Host "Installing MSYS2 packages via pacman: toolchain, boost, openssl, sqlite3, ninja, nsis"
    & $pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-nsis
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
