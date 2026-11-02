#!/usr/bin/env python3
"""
SignalStack UHF Bridge  v1.0
Reads EPC Gen2 tags from Jence J4212U and forwards to Flipper Zero via USB-TTL adapter.

Wiring (USB-to-TTL adapter → Flipper GPIO):
  Adapter TX  →  Flipper pin 14  (RX)
  Adapter RX  →  Flipper pin 13  (TX)
  Adapter GND →  Flipper GND     (pin 8 or 11)

Usage:
  python3 signalstack_bridge.py <JENCE_COM> <FLIPPER_UART_COM> [baud]
  python3 signalstack_bridge.py COM3 COM5
  python3 signalstack_bridge.py COM3 COM5 115200

Requirements:
  pip install pyserial
  j4210u.dll must be in the same folder as this script
  (get it from https://github.com/jence/j4210u-app  platform/windows/)
"""

import ctypes
import os
import sys
import time
import serial
import struct
from ctypes import c_bool, c_char_p, c_int, c_int8, c_uint8, c_uint32, Structure

DEBOUNCE_S   = 2.0   # seconds before same EPC can be reported again
SCAN_SLEEP_S = 0.08  # seconds between inventory calls


# ── J4210U native library wrapper ─────────────────────────────────────────────

class ScanResult(Structure):
    _fields_ = [
        ("Ant",       c_uint8),
        ("RSSI",      c_int8),
        ("Count",     c_uint8),
        ("EpcLength", c_uint8),
        ("EPC",       c_uint8 * 12),
    ]

def hex_epc(sr: ScanResult) -> str:
    return "".join(f"{b:02X}" for b in sr.EPC[: sr.EpcLength])


def load_j4210u_dll() -> ctypes.CDLL:
    search = [
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "j4210u.dll"),
        os.path.join(os.getcwd(), "j4210u.dll"),
        r"C:\Program Files\J4210U\j4210u.dll",
        "j4210u.dll",
    ]
    for path in search:
        if os.path.exists(path):
            print(f"[bridge] Loaded DLL: {path}")
            dll = ctypes.CDLL(path)
            _bind(dll)
            return dll
    print("[ERROR] j4210u.dll not found.")
    print("  Copy it next to this script from:")
    print("  https://github.com/jence/j4210u-app  →  platform/windows/j4210u.dll")
    sys.exit(1)


def _bind(dll: ctypes.CDLL):
    dll.OpenComPort.restype  = c_bool
    dll.OpenComPort.argtypes = [c_char_p, c_int]
    dll.CloseComPort.restype = None
    dll.CloseComPort.argtypes = []
    dll.Inventory.restype    = c_int
    dll.Inventory.argtypes   = [c_int]
    dll.GetResult.restype    = c_int
    dll.GetResult.argtypes   = [c_int, ctypes.c_char_p, c_int]


# ── Bridge main loop ───────────────────────────────────────────────────────────

def run_bridge(jence_port: str, flipper_port: str, jence_baud: int = 57600):
    dll = load_j4210u_dll()

    # Open jence reader
    print(f"[bridge] Opening Jence on {jence_port} @ {jence_baud} baud...")
    if not dll.OpenComPort(jence_port.encode(), jence_baud):
        if jence_baud == 57600:
            print("[bridge] 57600 failed, trying 115200...")
            if not dll.OpenComPort(jence_port.encode(), 115200):
                print(f"[ERROR] Cannot open Jence on {jence_port}. Check port and power.")
                sys.exit(1)
        else:
            print(f"[ERROR] Cannot open Jence on {jence_port}.")
            sys.exit(1)
    print("[bridge] Jence connected.")

    # Open Flipper UART serial port
    print(f"[bridge] Opening Flipper UART on {flipper_port} @ 115200 baud...")
    try:
        fp = serial.Serial(flipper_port, 115200, timeout=0.05)
    except Exception as exc:
        print(f"[ERROR] Cannot open {flipper_port}: {exc}")
        dll.CloseComPort()
        sys.exit(1)
    print("[bridge] Flipper UART connected.")
    print("[bridge] Bridging. Press Ctrl+C to stop.\n")

    # {epc_hex: last_seen_time}
    debounce: dict[str, float] = {}
    buf = ctypes.create_string_buffer(64)

    try:
        while True:
            now = time.monotonic()
            count = dll.Inventory(0)

            for i in range(count):
                dll.GetResult(i, buf, 64)
                sr = ScanResult.from_buffer_copy(buf.raw[:16])
                epc = hex_epc(sr)
                if not epc:
                    continue

                last = debounce.get(epc, 0.0)
                if now - last < DEBOUNCE_S:
                    continue

                debounce[epc] = now
                line = f"UHF,{epc},{sr.RSSI},{sr.Ant}\n"
                try:
                    fp.write(line.encode())
                    fp.flush()
                except serial.SerialException as exc:
                    print(f"[WARN] Flipper UART write failed: {exc}")

                print(f"  TAG  EPC:{epc:24s}  RSSI:{sr.RSSI:4d} dBm  ANT:{sr.Ant}")

            # Purge very old debounce entries to avoid memory growth
            if len(debounce) > 500:
                cutoff = now - DEBOUNCE_S * 10
                debounce = {k: v for k, v in debounce.items() if v > cutoff}

            time.sleep(SCAN_SLEEP_S)

    except KeyboardInterrupt:
        print("\n[bridge] Stopped.")
    finally:
        try:
            fp.close()
        except Exception:
            pass
        dll.CloseComPort()


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    jence_port   = sys.argv[1]
    flipper_port = sys.argv[2]
    jence_baud   = int(sys.argv[3]) if len(sys.argv) >= 4 else 57600

    run_bridge(jence_port, flipper_port, jence_baud)
