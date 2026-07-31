"""ace-os command line: download the ISO and make a bootable USB drive."""

import argparse
import platform
import sys
from pathlib import Path

from . import __version__
from .download import DownloadError, download_iso, ISO_NAME
from .flash import Drive, FlashError, flash, list_drives, print_progress


def _default_dest() -> Path:
    downloads = Path.home() / "Downloads"
    return downloads if downloads.is_dir() else Path.cwd()


def _pick_drive(preselected=None) -> Drive:
    drives = list_drives()
    if not drives:
        raise FlashError(
            "No USB drive found. Plug in a USB stick (8 GB or larger) and try again."
        )
    if preselected is not None:
        for drive in drives:
            if drive.id == str(preselected) or drive.raw == str(preselected):
                return drive
        raise FlashError(f"--drive {preselected} does not match any USB drive.")
    print("\nUSB drives found:")
    for index, drive in enumerate(drives, 1):
        print(f"  {index}. {drive.display}")
    while True:
        choice = input(f"Which drive do you want to use? [1-{len(drives)}] ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(drives):
            return drives[int(choice) - 1]
        print("Please enter one of the listed numbers.")


def _confirm_and_flash(drive: Drive, iso: Path):
    print(f"\nAbout to write {iso.name} to:")
    print(f"  {drive.display}")
    print("EVERYTHING on that drive will be permanently erased.")
    answer = input("Type ERASE (in capitals) to continue: ").strip()
    if answer != "ERASE":
        print("Cancelled — nothing was written.")
        return False
    flash(drive, iso, progress=print_progress)
    print(
        "\nDone! The USB drive is ready.\n"
        "Next: plug it into the ASUS laptop, tap F2 at power-on to disable\n"
        "Secure Boot, then tap Esc to boot from the USB drive. Full guide:\n"
        "https://github.com/gavincason1234-create/ace-os/blob/main/docs/install-asus-l510m.md"
    )
    return True


def _find_local_iso(args) -> Path:
    if getattr(args, "iso", None):
        return Path(args.iso).expanduser()
    candidate = _default_dest() / ISO_NAME
    if candidate.is_file():
        return candidate
    raise FlashError(
        f"No ISO found at {candidate}. Run 'ace-os download' first, "
        "or pass --iso PATH."
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="ace-os",
        description=(
            "Get ACE OS onto a USB stick. With no arguments this downloads the "
            "latest ISO and writes it to a USB drive, interactively."
        ),
    )
    parser.add_argument("--version", action="version", version=__version__)
    sub = parser.add_subparsers(dest="command")

    make = sub.add_parser("make-usb", help="download the ISO and write the USB (default)")
    make.add_argument("--dest", help="folder to store the ISO (default: Downloads)")
    make.add_argument("--drive", help="target drive id (skip the interactive picker)")

    down = sub.add_parser("download", help="only download the latest ISO")
    down.add_argument("--dest", help="folder to store the ISO (default: Downloads)")

    fl = sub.add_parser("flash", help="only write an existing ISO to a USB drive")
    fl.add_argument("--iso", help=f"path to the ISO (default: Downloads/{ISO_NAME})")
    fl.add_argument("--drive", help="target drive id (skip the interactive picker)")

    sub.add_parser("list-drives", help="show USB drives that can be used")

    args = parser.parse_args(argv)
    command = args.command or "make-usb"

    try:
        if command == "list-drives":
            drives = list_drives()
            if not drives:
                print("No USB drives found.")
            for drive in drives:
                print(f"  {drive.display}")
            return 0

        if command == "download":
            dest = Path(getattr(args, "dest", None) or _default_dest())
            download_iso(dest)
            return 0

        if command == "flash":
            iso = _find_local_iso(args)
            drive = _pick_drive(getattr(args, "drive", None))
            return 0 if _confirm_and_flash(drive, iso) else 1

        # make-usb (default): the whole thing, end to end.
        dest = Path(getattr(args, "dest", None) or _default_dest())
        iso = download_iso(dest)
        drive = _pick_drive(getattr(args, "drive", None))
        return 0 if _confirm_and_flash(drive, iso) else 1

    except (DownloadError, FlashError) as err:
        print(f"\nerror: {err}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nCancelled.", file=sys.stderr)
        return 130
    except PermissionError:
        hint = (
            "run the terminal as Administrator"
            if platform.system() == "Windows"
            else "re-run with sudo"
        )
        print(f"\nerror: permission denied — {hint}.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
