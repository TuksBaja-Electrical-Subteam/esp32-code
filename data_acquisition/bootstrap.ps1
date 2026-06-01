# bootstrap.ps1
# Run this once per terminal session
# Usage: . .\bootstrap.ps1  (the leading dot is important, same reason as 'source')

$IDF_PATH = "$env:USERPROFILE\esp\v5.5.1\esp-idf"

if (-not (Test-Path $IDF_PATH)) {
    Write-Error "ESP-IDF not found at $IDF_PATH"
    Write-Host "Install it first: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/"
    return
}

Write-Host "Activating ESP-IDF environment..."
. "$IDF_PATH\export.ps1"
Write-Host "Done. You can now use idf.py"