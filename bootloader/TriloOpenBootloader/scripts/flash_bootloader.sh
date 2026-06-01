#!/bin/bash
# Interactive bootloader flashing script wrapper

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/flash_bootloader.py"

if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo "❌ Error: flash_bootloader.py not found at $PYTHON_SCRIPT"
    exit 1
fi

python3 "$PYTHON_SCRIPT"
