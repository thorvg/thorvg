@echo off
setlocal

rem Work in the folder where this .bat lives
cd /d "%~dp0"

rem Get arg; default to sw if none
set "ARG=%~1"
if not defined ARG set "ARG=sw"

rem Validate
if /I not "%ARG%"=="gl" if /I not "%ARG%"=="sw" if /I not "%ARG%"=="wg" (
  echo Invalid argument: %ARG%
  echo Usage: %~n0 [gl^|sw^|wg]
  exit /b 1
)

for %%F in ("*.exe") do (
  echo Launching %%~nxF %ARG%
  start "" "%%~fF" %ARG%
  timeout /t 2 /nobreak >nul
  rem Try to close it, then force-close if needed
  taskkill /im "%%~nxF" /t >nul 2>&1
  taskkill /im "%%~nxF" /f /t >nul 2>&1
)

echo Done.
endlocal
