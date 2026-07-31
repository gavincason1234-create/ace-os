"""Write an ISO to a removable USB drive on Windows, macOS, or Linux.

Safety rules: only removable/USB disks are ever listed as targets, and the
CLI requires the user to type ERASE before anything is written.
"""

import ctypes
import json
import os
import platform
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

CHUNK = 4 * 1024 * 1024
SECTOR = 4096  # pad writes to this alignment; raw devices require it on Windows


class FlashError(RuntimeError):
    pass


@dataclass
class Drive:
    id: str       # /dev/sdb, /dev/disk2, or a Windows disk number
    display: str  # human-readable description
    size: int     # bytes
    raw: str      # device path opened for writing


def _fmt_size(size):
    return f"{size / 1e9:.1f} GB"


def list_drives():
    system = platform.system()
    if system == "Linux":
        return _list_linux()
    if system == "Darwin":
        return _list_macos()
    if system == "Windows":
        return _list_windows()
    raise FlashError(f"Unsupported operating system: {system}")


def flash(drive: Drive, iso: Path, progress=None):
    system = platform.system()
    iso = Path(iso)
    if not iso.is_file():
        raise FlashError(f"ISO not found: {iso}")
    if iso.stat().st_size > drive.size:
        raise FlashError(
            f"The USB drive ({_fmt_size(drive.size)}) is smaller than the ISO "
            f"({_fmt_size(iso.stat().st_size)}). Use an 8 GB or larger drive."
        )
    if system == "Linux":
        _flash_linux(drive, iso, progress)
    elif system == "Darwin":
        _flash_macos(drive, iso, progress)
    elif system == "Windows":
        _flash_windows(drive, iso, progress)
    else:
        raise FlashError(f"Unsupported operating system: {system}")


def _raw_copy(iso: Path, fd, progress):
    total = iso.stat().st_size
    done = 0
    with open(iso, "rb") as src:
        while True:
            chunk = src.read(CHUNK)
            if not chunk:
                break
            if len(chunk) % SECTOR:
                chunk += b"\0" * (SECTOR - len(chunk) % SECTOR)
            os.write(fd, chunk)
            done += len(chunk)
            if progress:
                progress(min(done, total), total)
    os.fsync(fd)


# ---------------------------------------------------------------- Linux

def _list_linux():
    out = subprocess.run(
        ["lsblk", "-J", "-b", "-d", "-o", "NAME,SIZE,RM,TYPE,MODEL,TRAN"],
        capture_output=True, text=True, check=True,
    ).stdout
    drives = []
    for dev in json.loads(out).get("blockdevices", []):
        if dev.get("type") != "disk":
            continue
        if not (dev.get("rm") in (True, 1, "1") or dev.get("tran") == "usb"):
            continue
        path = "/dev/" + dev["name"]
        size = int(dev.get("size") or 0)
        model = (dev.get("model") or "USB drive").strip()
        drives.append(Drive(path, f"{path}  {model}  {_fmt_size(size)}", size, path))
    return drives


def _require_root():
    if os.geteuid() != 0:
        raise FlashError(
            "Writing to a USB drive needs root. Re-run the same command with sudo."
        )


def _flash_linux(drive, iso, progress):
    _require_root()
    out = subprocess.run(
        ["lsblk", "-J", "-o", "NAME,MOUNTPOINT", drive.id],
        capture_output=True, text=True, check=True,
    ).stdout

    def _unmount(node):
        if node.get("mountpoint"):
            subprocess.run(["umount", "/dev/" + node["name"]], check=False)
        for child in node.get("children", []):
            _unmount(child)

    for node in json.loads(out).get("blockdevices", []):
        _unmount(node)

    fd = os.open(drive.raw, os.O_WRONLY | getattr(os, "O_SYNC", 0))
    try:
        _raw_copy(iso, fd, progress)
    finally:
        os.close(fd)


# ---------------------------------------------------------------- macOS

def _list_macos():
    import plistlib

    out = subprocess.run(
        ["diskutil", "list", "-plist", "external", "physical"],
        capture_output=True, check=True,
    ).stdout
    drives = []
    for disk in plistlib.loads(out).get("AllDisksAndPartitions", []):
        ident = disk["DeviceIdentifier"]
        size = int(disk.get("Size") or 0)
        drives.append(
            Drive(
                "/dev/" + ident,
                f"/dev/{ident}  external disk  {_fmt_size(size)}",
                size,
                "/dev/r" + ident,
            )
        )
    return drives


def _flash_macos(drive, iso, progress):
    _require_root()
    subprocess.run(
        ["diskutil", "unmountDisk", "force", drive.id],
        check=True, capture_output=True,
    )
    fd = os.open(drive.raw, os.O_WRONLY)
    try:
        _raw_copy(iso, fd, progress)
    finally:
        os.close(fd)


# ---------------------------------------------------------------- Windows

_PS = ["powershell", "-NoProfile", "-NonInteractive", "-Command"]


def _list_windows():
    cmd = (
        "Get-Disk | Where-Object { $_.BusType -eq 'USB' } | "
        "Select-Object Number,FriendlyName,Size | ConvertTo-Json"
    )
    out = subprocess.run(_PS + [cmd], capture_output=True, text=True, check=True).stdout.strip()
    if not out:
        return []
    data = json.loads(out)
    if isinstance(data, dict):
        data = [data]
    drives = []
    for disk in data:
        num = disk["Number"]
        size = int(disk.get("Size") or 0)
        name = (disk.get("FriendlyName") or "USB drive").strip()
        drives.append(
            Drive(
                str(num),
                f"Disk {num}  {name}  {_fmt_size(size)}",
                size,
                rf"\\.\PhysicalDrive{num}",
            )
        )
    return drives


def _flash_windows(drive, iso, progress):
    if not ctypes.windll.shell32.IsUserAnAdmin():
        raise FlashError(
            "Writing to a USB drive needs admin rights.\n"
            "Close this window, right-click Command Prompt -> 'Run as "
            "administrator', and run the command again."
        )

    kernel32 = ctypes.windll.kernel32
    kernel32.CreateFileW.restype = ctypes.c_void_p
    kernel32.DeviceIoControl.argtypes = [
        ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_ulong,
        ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_void_p,
    ]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    INVALID_HANDLE = ctypes.c_void_p(-1).value
    GENERIC_RW = 0x80000000 | 0x40000000
    SHARE_RW = 0x00000001 | 0x00000002
    OPEN_EXISTING = 3
    FSCTL_LOCK_VOLUME = 0x00090018
    FSCTL_DISMOUNT_VOLUME = 0x00090020

    # Lock and dismount every mounted volume on the target disk so Windows
    # doesn't fight us (or pop "format this disk?" dialogs) mid-write.
    cmd = (
        f"Get-Partition -DiskNumber {drive.id} -ErrorAction SilentlyContinue | "
        "Where-Object DriveLetter | Select-Object -ExpandProperty DriveLetter"
    )
    letters = subprocess.run(
        _PS + [cmd], capture_output=True, text=True, check=False
    ).stdout.split()

    volume_handles = []
    try:
        for letter in letters:
            handle = kernel32.CreateFileW(
                rf"\\.\{letter}:", GENERIC_RW, SHARE_RW, None, OPEN_EXISTING, 0, None
            )
            if handle and handle != INVALID_HANDLE:
                returned = ctypes.c_ulong(0)
                kernel32.DeviceIoControl(
                    handle, FSCTL_LOCK_VOLUME, None, 0, None, 0,
                    ctypes.byref(returned), None,
                )
                kernel32.DeviceIoControl(
                    handle, FSCTL_DISMOUNT_VOLUME, None, 0, None, 0,
                    ctypes.byref(returned), None,
                )
                volume_handles.append(handle)

        fd = os.open(drive.raw, os.O_RDWR | os.O_BINARY)
        try:
            _raw_copy(iso, fd, progress)
        finally:
            os.close(fd)
    finally:
        for handle in volume_handles:
            kernel32.CloseHandle(handle)


def print_progress(done, total):
    pct = 100 * done // total if total else 0
    bar = "#" * (pct // 4)
    sys.stdout.write(f"\r  writing  [{bar:<25}] {pct:3d}%  ({done // (1024*1024)} MB)")
    sys.stdout.flush()
    if done >= total:
        sys.stdout.write("\n")
