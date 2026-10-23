@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\start-oj.ps1" -Stop -NoBrowser
if errorlevel 1 (
    pause
    exit /b 1
)
endlocal
exit /b 0
