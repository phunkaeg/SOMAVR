@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SOMAVR_PACKAGE=%~dp0out\SOMAVR-latest"
set "SOMA_EXE=G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
set "INJECTOR=%SOMAVR_PACKAGE%\somavr_injector.exe"
set "VR_DLL=%SOMAVR_PACKAGE%\somavr.dll"
set "RESULT=1"

if not "%~1"=="" if /i not "%~1"=="--check" goto usage
if not exist "%INJECTOR%" (
    echo Missing injector: "%INJECTOR%"
    goto failed
)
if not exist "%VR_DLL%" (
    echo Missing VR DLL: "%VR_DLL%"
    goto failed
)
if not exist "%SOMA_EXE%" (
    echo Missing game: "%SOMA_EXE%"
    goto failed
)
if /i "%~1"=="--check" goto check

powershell.exe -NoLogo -NoProfile -NonInteractive -Command "$ErrorActionPreference = 'Stop'; if (Get-Process -Name Soma,Soma_NoSteam -ErrorAction SilentlyContinue) { Write-Host 'SOMA is already running. Close it before launching again.'; exit 1 }; exit 0"
if errorlevel 1 goto failed

echo Launching SOMAVR from "%SOMAVR_PACKAGE%"
"%INJECTOR%" --launch "%SOMA_EXE%" "%VR_DLL%"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto failed
exit /b 0

:check
"%INJECTOR%" --doctor "%SOMA_EXE%" "%VR_DLL%"
exit /b %ERRORLEVEL%

:usage
echo Usage: Launch-SOMAVR.bat [--check]
echo --check runs readiness checks without launching the game.
exit /b 2

:failed
echo SOMAVR was not launched successfully.
if /i not "%~1"=="--check" pause
exit /b %RESULT%
