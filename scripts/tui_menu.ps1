Write-Host "1) Debug"
Write-Host "2) Release"
$choice = Read-Host "Build type"

if ($choice -eq "1") {
    Write-Output "BUILD_TYPE=Debug"
} else {
    Write-Output "BUILD_TYPE=Release"
}

$tests = Read-Host "Enable tests? (y/n)"
if ($tests.Trim().ToLower() -in @("y", "yes")) {
    Write-Output "ENABLE_TESTS=ON"
} else {
    Write-Output "ENABLE_TESTS=OFF"
}
