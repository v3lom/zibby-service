@echo off
setlocal

REM Prefer UTF-8 in classic cmd.exe to render Cyrillic/box-drawing correctly
chcp 65001 >nul

powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set EXIT_CODE=%ERRORLEVEL%

exit /b %EXIT_CODE%
