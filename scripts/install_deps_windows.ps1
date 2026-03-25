$ErrorActionPreference = "Stop"

function Use-Winget {
    winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
    winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements
    winget install --id LLVM.LLVM -e --accept-package-agreements --accept-source-agreements
}

function Use-Choco {
    choco install -y cmake git llvm
}

if (Get-Command winget -ErrorAction SilentlyContinue) {
    Use-Winget
} elseif (Get-Command choco -ErrorAction SilentlyContinue) {
    Use-Choco
} else {
    Write-Host "Neither winget nor choco found. Install dependencies manually."
    exit 1
}

Write-Host "Install Boost, OpenSSL, SQLite and Catch2 with vcpkg or MSYS2 for your toolchain."
