#!/bin/bash
# SignalStack NFC — build and flash to Flipper Zero (Momentum firmware)
# Usage: ./build_and_flash.sh [--no-flash]
#
# Requirements:
#   - ufbt installed: pip install ufbt
#   - Flipper Zero connected via USB
#   - Momentum firmware flashed to Flipper

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "  SignalStack NFC — Build & Flash"
echo "=========================================="

# Check ufbt
if ! command -v ufbt &>/dev/null; then
    echo "[!] ufbt not found. Installing..."
    pip install ufbt
fi

# Update ufbt SDK (uses Momentum/official Flipper SDK)
echo "[*] Updating Flipper SDK..."
ufbt update

# Build the FAP
echo "[*] Building signalstack_nfc.fap ..."
ufbt build

echo "[+] Build complete!"
ls -lh .ufbt/build/signalstack_nfc.fap 2>/dev/null || \
    find . -name "*.fap" -exec ls -lh {} \;

if [[ "$1" != "--no-flash" ]]; then
    echo "[*] Flashing to Flipper Zero..."
    # Launch app directly after flashing
    ufbt launch
    echo "[+] Done! SignalStack NFC should be running on your Flipper."
fi
