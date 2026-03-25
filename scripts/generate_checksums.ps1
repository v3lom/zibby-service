param(
    [string]$ArtifactsDir = "installers"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $ArtifactsDir)) {
    New-Item -ItemType Directory -Path $ArtifactsDir | Out-Null
}

$patterns = @("*.zip", "*.tar.gz", "*.deb", "*.rpm", "*.exe")
$files = Get-ChildItem -Path $ArtifactsDir -File | Where-Object {
    $name = $_.Name.ToLowerInvariant()
    $patterns | ForEach-Object { if ($name -like $_) { return $true } }
    return $false
} | Sort-Object Name

$outFile = Join-Path $ArtifactsDir "SHA256SUMS.txt"

$lines = foreach ($file in $files) {
    $hash = Get-FileHash -Path $file.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($file.Name)"
}

$lines | Set-Content -Path $outFile -Encoding UTF8
Write-Host "Checksums written to $outFile"
