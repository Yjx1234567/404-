@echo off
chcp 65001 >nul
title Step 1 - Install WSL + Ubuntu 22.04

echo.
echo  ============================================
echo   STEP 1 / 3 :  Install WSL + Ubuntu 22.04
echo.
echo   IMPORTANT : we use Ubuntu 22.04, NOT 24.04.
echo   Reason : Ubuntu 24.04 no longer provides
echo   libbotan-2-dev, which this project needs.
echo   On 22.04 everything works out of the box.
echo.
echo   - If Windows asks for permission : click YES
echo   - Takes a few minutes : please wait
echo   - Do NOT close this window
echo  ============================================
echo.

net session >nul 2>&1
if not %errorlevel% == 0 (
    echo Asking for administrator permission ...
    echo click YES in the popup window.
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo Currently installed Linux distributions :
echo ------------------------------------------
wsl -l -v
echo ------------------------------------------
echo.
echo If you see  Ubuntu  (without a version number)
echo in the list above, that is the WRONG one
echo (Ubuntu 24.04). Run  0-uninstall-old-Ubuntu.bat
echo first, then come back to this file.
echo.

echo Running command :  wsl --install -d Ubuntu-22.04
echo.
wsl --install -d Ubuntu-22.04

echo.
echo  ============================================
echo   STEP 1 finished.
echo.
echo   1. If it says RESTART is required :
echo      restart the computer now.
echo.
echo   2. After restart : open Start Menu,
echo      type  ubuntu , open  Ubuntu-22.04 .
echo.
echo   3. It asks for a new username (lowercase)
echo      and a password.
echo      NOTE : the password is INVISIBLE while
echo      typing. Just type it and press Enter.
echo.
echo   4. Then come back to this project folder
echo      and double-click the file whose name
echo      starts with  2-
echo  ============================================
pause
