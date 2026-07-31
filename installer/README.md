# aceos-installer

One command to get [ACE OS](https://github.com/gavincason1234-create/ace-os)
onto a USB stick: it downloads the latest ACE OS ISO and writes it to a USB
drive, ready to boot on the ASUS VivoBook L510M.

```sh
ace-os            # download the ISO + make the bootable USB, interactively
ace-os download   # just download the ISO
ace-os flash      # just write an already-downloaded ISO to USB
ace-os list-drives
```

Writing to a USB drive needs elevated rights: run the terminal **as
Administrator** on Windows, or with `sudo` on Linux/macOS. The tool only
offers removable USB drives as targets and asks you to type `ERASE` before
touching anything.

Uses only the Python standard library — no dependencies.
