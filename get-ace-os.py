#!/usr/bin/env python3
"""Get ACE OS onto a USB stick — one file, no pip needed.

How to use:
  Windows:  open Command Prompt AS ADMINISTRATOR, then:
                py get-ace-os.py
            (if "py" is not recognized, install Python: type python, press
             Enter, and install it from the Microsoft Store that opens; then
             close this window, open a new Administrator one, and retry)
  Linux/Mac:  sudo python3 get-ace-os.py

What it does: downloads the latest ACE OS ISO from GitHub (about 1.8 GB),
verifies it, then writes it to a USB drive you pick. Only removable USB
drives are offered, and you must type ERASE before anything is written.

Optional: `py get-ace-os.py download` only downloads the ISO.

This file mirrors the aceos-installer pip package (installer/src/) in a
single script for people who can't use pip. Python 3.8+ and the standard
library only.
"""

import ctypes
import hashlib
import json
import os
import platform
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

OWNER = "gavincason1234-create"
REPO = "ace-os"
DOWNLOAD_BASE = f"https://github.com/{OWNER}/{REPO}/releases/latest/download"
GUIDE_URL = f"https://github.com/{OWNER}/{REPO}/blob/main/docs/install-asus-l510m.md"
ISO_NAME = "ace-os-amd64.iso"
CHUNK = 1024 * 1024
SECTOR = 4096


class AceError(RuntimeError):
    pass


# ------------------------------------------------------------- download

def _open(url):
    req = urllib.request.Request(url, headers={"User-Agent": "get-ace-os"})
    return urllib.request.urlopen(req)


def _progress(name, done, total):
    if total:
        pct = 100 * done // total
        bar = "#" * (pct // 4)
        sys.stdout.write(f"\r  {name}  [{bar:<25}] {pct:3d}%  ({done // (1024*1024)} MB)")
    else:
        sys.stdout.write(f"\r  {name}  {done // (1024*1024)} MB")
    sys.stdout.flush()


def _stream_to(resp, dest: Path):
    total = int(resp.headers.get("Content-Length") or 0)
    done = 0
    with open(dest, "wb") as out:
        while True:
            chunk = resp.read(CHUNK)
            if not chunk:
                break
            out.write(chunk)
            done += len(chunk)
            _progress(dest.name, done, total)
    sys.stdout.write("\n")


def _sha256(path: Path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _verify(path: Path, expected, label):
    print(f"Verifying {label}...")
    if _sha256(path) != expected:
        path.unlink()
        raise AceError(f"Checksum mismatch on {label} — the download is corrupt. Try again.")


def download_iso(dest_dir: Path) -> Path:
    dest_dir = Path(dest_dir).expanduser()
    dest_dir.mkdir(parents=True, exist_ok=True)
    iso_path = dest_dir / ISO_NAME

    print("Checking for the latest ACE OS release...")
    try:
        with _open(f"{DOWNLOAD_BASE}/SHA256SUMS") as resp:
            text = resp.read().decode()
    except urllib.error.HTTPError as err:
        if err.code in (401, 403, 404):
            raise AceError(
                "Could not find a published ACE OS ISO. Run the 'Build ACE OS ISO'\n"
                "workflow in the repo's Actions tab first, and make sure the repo\n"
                "is public."
            )
        raise

    sums = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 2:
            sums[parts[-1].lstrip("*")] = parts[0]

    expected = sums.get(ISO_NAME)
    if not expected:
        raise AceError("The release's SHA256SUMS does not list the ISO — rebuild it.")

    if iso_path.exists() and _sha256(iso_path) == expected:
        print(f"Already downloaded and verified: {iso_path}")
        return iso_path

    parts = sorted(name for name in sums if name.endswith(".part"))
    tmp = iso_path.with_suffix(".iso.download")

    if parts:
        print(f"Downloading the ACE OS ISO ({len(parts)} parts)...")
        with open(tmp, "wb") as out:
            for name in parts:
                part_path = dest_dir / name
                with _open(f"{DOWNLOAD_BASE}/{name}") as resp:
                    _stream_to(resp, part_path)
                _verify(part_path, sums[name], name)
                with open(part_path, "rb") as pf:
                    while True:
                        chunk = pf.read(CHUNK)
                        if not chunk:
                            break
                        out.write(chunk)
                part_path.unlink()
    else:
        print("Downloading the ACE OS ISO (about 1.8 GB — this takes a while)...")
        with _open(f"{DOWNLOAD_BASE}/{ISO_NAME}") as resp:
            _stream_to(resp, tmp)

    _verify(tmp, expected, ISO_NAME)
    print("Checksum OK.")
    tmp.replace(iso_path)
    print(f"ISO ready: {iso_path}")
    return iso_path


# ------------------------------------------------------------- drives

class Drive:
    def __init__(self, id, display, size, raw):
        self.id, self.display, self.size, self.raw = id, display, size, raw


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
    raise AceError(f"Unsupported operating system: {system}")


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
        drives.append(Drive("/dev/" + ident, f"/dev/{ident}  external disk  {_fmt_size(size)}",
                            size, "/dev/r" + ident))
    return drives


_PS = ["powershell", "-NoProfile", "-NonInteractive", "-Command"]


def _list_windows():
    cmd = ("Get-Disk | Where-Object { $_.BusType -eq 'USB' } | "
           "Select-Object Number,FriendlyName,Size | ConvertTo-Json")
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
        drives.append(Drive(str(num), f"Disk {num}  {name}  {_fmt_size(size)}",
                            size, rf"\\.\PhysicalDrive{num}"))
    return drives


# ------------------------------------------------------------- flashing

def _raw_copy(iso: Path, fd):
    total = iso.stat().st_size
    done = 0
    with open(iso, "rb") as src:
        while True:
            chunk = src.read(4 * CHUNK)
            if not chunk:
                break
            if len(chunk) % SECTOR:
                chunk += b"\0" * (SECTOR - len(chunk) % SECTOR)
            os.write(fd, chunk)
            done += len(chunk)
            _progress("writing", min(done, total), total)
    os.fsync(fd)
    sys.stdout.write("\n")


def _require_root():
    if os.geteuid() != 0:
        raise AceError("Writing to a USB drive needs root. Re-run with sudo.")


def flash(drive: Drive, iso: Path):
    if iso.stat().st_size > drive.size:
        raise AceError(
            f"The USB drive ({_fmt_size(drive.size)}) is smaller than the ISO "
            f"({_fmt_size(iso.stat().st_size)}). Use an 8 GB or larger drive."
        )
    system = platform.system()
    if system == "Linux":
        _require_root()
        out = subprocess.run(["lsblk", "-J", "-o", "NAME,MOUNTPOINT", drive.id],
                             capture_output=True, text=True, check=True).stdout

        def _unmount(node):
            if node.get("mountpoint"):
                subprocess.run(["umount", "/dev/" + node["name"]], check=False)
            for child in node.get("children", []):
                _unmount(child)

        for node in json.loads(out).get("blockdevices", []):
            _unmount(node)
        fd = os.open(drive.raw, os.O_WRONLY | getattr(os, "O_SYNC", 0))
        try:
            _raw_copy(iso, fd)
        finally:
            os.close(fd)

    elif system == "Darwin":
        _require_root()
        subprocess.run(["diskutil", "unmountDisk", "force", drive.id],
                       check=True, capture_output=True)
        fd = os.open(drive.raw, os.O_WRONLY)
        try:
            _raw_copy(iso, fd)
        finally:
            os.close(fd)

    elif system == "Windows":
        if not ctypes.windll.shell32.IsUserAnAdmin():
            raise AceError(
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
        INVALID = ctypes.c_void_p(-1).value
        GENERIC_RW = 0x80000000 | 0x40000000
        SHARE_RW = 0x00000001 | 0x00000002
        OPEN_EXISTING = 3
        FSCTL_LOCK_VOLUME = 0x00090018
        FSCTL_DISMOUNT_VOLUME = 0x00090020

        cmd = (f"Get-Partition -DiskNumber {drive.id} -ErrorAction SilentlyContinue | "
               "Where-Object DriveLetter | Select-Object -ExpandProperty DriveLetter")
        letters = subprocess.run(_PS + [cmd], capture_output=True, text=True,
                                 check=False).stdout.split()
        handles = []
        try:
            for letter in letters:
                handle = kernel32.CreateFileW(rf"\\.\{letter}:", GENERIC_RW, SHARE_RW,
                                              None, OPEN_EXISTING, 0, None)
                if handle and handle != INVALID:
                    returned = ctypes.c_ulong(0)
                    kernel32.DeviceIoControl(handle, FSCTL_LOCK_VOLUME, None, 0,
                                             None, 0, ctypes.byref(returned), None)
                    kernel32.DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, None, 0,
                                             None, 0, ctypes.byref(returned), None)
                    handles.append(handle)
            fd = os.open(drive.raw, os.O_RDWR | os.O_BINARY)
            try:
                _raw_copy(iso, fd)
            finally:
                os.close(fd)
        finally:
            for handle in handles:
                kernel32.CloseHandle(handle)
    else:
        raise AceError(f"Unsupported operating system: {system}")


# ------------------------------------------------------------- main

def main():
    download_only = len(sys.argv) > 1 and sys.argv[1] == "download"
    downloads = Path.home() / "Downloads"
    dest = downloads if downloads.is_dir() else Path.cwd()

    try:
        iso = download_iso(dest)
        if download_only:
            return 0

        drives = list_drives()
        if not drives:
            raise AceError(
                "No USB drive found. Plug in a USB stick (8 GB or larger) and "
                "run this again."
            )
        print("\nUSB drives found:")
        for index, drive in enumerate(drives, 1):
            print(f"  {index}. {drive.display}")
        while True:
            choice = input(f"Which drive do you want to use? [1-{len(drives)}] ").strip()
            if choice.isdigit() and 1 <= int(choice) <= len(drives):
                drive = drives[int(choice) - 1]
                break
            print("Please enter one of the listed numbers.")

        print(f"\nAbout to write {iso.name} to:")
        print(f"  {drive.display}")
        print("EVERYTHING on that drive will be permanently erased.")
        if input("Type ERASE (in capitals) to continue: ").strip() != "ERASE":
            print("Cancelled — nothing was written.")
            return 1
        flash(drive, iso)
        print(
            "\nDone! The USB drive is ready.\n"
            "Next: plug it into the ASUS laptop, tap F2 at power-on to disable\n"
            f"Secure Boot, then tap Esc to boot from the USB drive. Full guide:\n{GUIDE_URL}"
        )
        return 0
    except AceError as err:
        print(f"\nerror: {err}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nCancelled.", file=sys.stderr)
        return 130
    except PermissionError:
        hint = ("run the terminal as Administrator" if platform.system() == "Windows"
                else "re-run with sudo")
        print(f"\nerror: permission denied — {hint}.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
