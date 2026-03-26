@echo off
setlocal

REM Prefer UTF-8 in classic cmd.exe to render Cyrillic/box-drawing correctly
chcp 65001 >nul

REM If a prebuilt TUI exists in the repo root and no args were provided,
REM run it directly (useful on PCs with restricted PowerShell policies).
if "%~1"=="" (
	if exist "%~dp0zibby-build-tui.exe" (
		"%~dp0zibby-build-tui.exe"
		exit /b %ERRORLEVEL%
	)
)

powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set EXIT_CODE=%ERRORLEVEL%

exit /b %EXIT_CODE%
