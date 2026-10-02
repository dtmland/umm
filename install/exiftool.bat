@echo off
rem Windows wrapper for install/exiftool.ps1 (concept §4.2).
rem Prefer winget OliverBetz.ExifTool; native cmd/PowerShell only.
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0exiftool.ps1" %*
exit /b %ERRORLEVEL%
