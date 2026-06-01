@echo off
REM bootstrap.bat
REM Run this once per terminal session
REM Usage: bootstrap.bat (runs directly, no 'call' needed from CMD)

set IDF_PATH=%USERPROFILE%\esp\v5.5.1\esp-idf

if not exist "%IDF_PATH%" (
    echo ERROR: ESP-IDF not found at %IDF_PATH%
    echo Install it first: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/
    exit /b 1
)

echo Activating ESP-IDF environment...
call "%IDF_PATH%\export.bat"
echo Done. You can now use idf.py