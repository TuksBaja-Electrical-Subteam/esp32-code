#!/bin/bash
# bootstrap.sh
# Run this once per terminal session before using idf.py
# Usage: source ./bootstrap.sh  (the 'source' is important — see below)

IDF_PATH="$HOME/esp/v5.5.1/esp-idf"

if [ ! -d "$IDF_PATH" ]; then
    echo "ERROR: ESP-IDF not found at $IDF_PATH"
    echo "Install it first: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/"
    return 1
fi

echo "Activating ESP-IDF environment..."
. "$IDF_PATH/export.sh"
echo "Done. You can now use idf.py"