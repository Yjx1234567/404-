@echo off
chcp 65001 >nul
title Optional - Remove the WRONG Ubuntu (24.04)

echo.
echo  ============================================
echo   OPTIONAL STEP 0 : Remove the old Ubuntu
echo.
echo   Only use this file if you already ran
echo   1-  and it installed  Ubuntu 24.04 .
echo   (24.04 cannot run this project.)
echo.
echo   ########################################
echo   #   !!  D A N G E R  !!                #
echo   #                                      #
echo   #   This DELETES the whole Linux       #
echo   #   system. Everything inside it       #
echo   #   (including MySQL data) is LOST.    #
echo   #                                      #
echo   #   Your WINDOWS files are NOT         #
echo   #   affected. This project folder on   #
echo   #   your Desktop is safe.              #
echo   ########################################
echo  ============================================
echo.

echo Installed distributions :
echo ------------------------------------------
wsl -l -v
echo ------------------------------------------
echo.

set /p ANSWER=Type YES (uppercase, 3 letters) then Enter to delete : 

if not "%ANSWER%" == "YES" (
    echo.
    echo Cancelled. Nothing was deleted.
    echo.
    pause
    exit /b
)

echo.
echo Deleting the distribution named "Ubuntu" ...
wsl --unregister Ubuntu

echo.
echo  ============================================
echo   Done.
echo.
echo   If you saw an error above, the distro has
echo   a different name. Open PowerShell and run:
echo.
echo       wsl -l -v
echo       wsl --unregister NAME_YOU_SAW
echo.
echo   Then double-click the file starting with
echo   1-  to install Ubuntu 22.04.
echo  ============================================
pause
