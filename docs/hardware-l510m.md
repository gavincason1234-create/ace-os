# Hardware notes — ASUS VivoBook L510M (L510MA-WSQ5)

The target machine for ACE OS 1.0.

| Component | Detail | Linux support |
| --- | --- | --- |
| CPU | Intel Celeron N4020, 2 cores, 1.1–2.8 GHz (Gemini Lake Refresh) | Fully supported (`intel-microcode` included) |
| RAM | 4 GB DDR4, soldered (not upgradeable) | The main constraint — ACE OS ships zram swap and a light XFCE desktop because of it |
| Storage | 128 GB eMMC (shows up as `/dev/mmcblk0`) | Supported by the `mmc_block` driver; TRIM handled by the weekly `fstrim.timer` |
| Graphics | Intel UHD 600 (integrated) | `i915` driver, works out of the box; `intel-media-va-driver` included for video acceleration |
| Display | 15.6" 1920×1080 | Works out of the box |
| Wi-Fi / Bluetooth | Intel wireless (802.11ac era, typically AC 9461/9462) | Needs `firmware-iwlwifi` — included on the ISO. `firmware-realtek`/`firmware-atheros` are also included in case a unit shipped with a different module |
| Sound | Intel HDA / SOF (Gemini Lake) | `firmware-sof-signed` included. If audio is silent, check the selected output device in the mixer first |
| Webcam | VGA webcam (USB internal) | `uvcvideo`, works out of the box |
| Keyboard hotkeys | Brightness, volume, etc. | Handled by `asus-wmi`, loaded automatically |

## Why these choices

- **XFCE desktop** — full-featured but comfortable in ~600 MB RAM at idle,
  leaving room for Firefox on a 4 GB machine.
- **zram swap** (`zstd`, 60% of RAM) — swapping to compressed RAM is far
  faster than swapping to eMMC and avoids wearing the eMMC out.
- **`vm.swappiness=100`** — with zram, swapping early is cheap and keeps the
  file cache warm on the slow eMMC.
- **Debian 13 (trixie) base** — stable, secure, and its 6.12 kernel has
  mature support for every component above (Gemini Lake has been well
  supported since Linux 4.x).

## Known quirks

- **Secure Boot**: the install guide has you disable it in the UEFI setup.
  This is the simplest reliable path on this machine.
- **eMMC naming**: partitions are `mmcblk0p1`, `mmcblk0p2`, … — not `sda1`.
- **Battery**: the L510M has a small 42 Wh battery; `xfce4-power-manager`
  and `task-laptop` power tooling are preinstalled.
