$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$bootstrapDir = Join-Path $root "build\bootstrap"
if (!(Test-Path $bootstrapDir)) {
    New-Item -ItemType Directory -Path $bootstrapDir | Out-Null
}

$cmakeArgs = @(
    "-S", $root,
    "-B", $bootstrapDir,
    "-DZIBBY_ENABLE_TESTS=OFF",
    "-DZIBBY_ENABLE_CALLS=OFF",
    "-DZIBBY_ENABLE_PLUGINS=OFF",
    "-DZIBBY_ENABLE_PANEL=OFF"
)

if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $cmakeArgs = @("-G", "Ninja") + $cmakeArgs
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $bootstrapDir --target zibby-build-tui
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $bootstrapDir "zibby-build-tui.exe"
& $exe
exit $LASTEXITCODE
