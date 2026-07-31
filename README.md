# ACE OS

A lightweight, ready-to-use Linux operating system built for the
**ASUS VivoBook L510M (L510MA-WSQ5)** — a laptop with a 2-core Celeron CPU,
4 GB of RAM, and 128 GB of eMMC storage. ACE OS is tuned so that exact
hardware feels quick instead of cramped.

Built on **Debian 13 (trixie)** with the **XFCE** desktop and the
**Calamares** graphical installer.

## What you get

- Fast, simple desktop (XFCE) that idles light and leaves RAM for Firefox
- Firefox, VLC, file manager, image viewer, text editor, archive tool
- All firmware for the L510M on the ISO: Intel Wi-Fi, Bluetooth, sound, graphics
- zram compressed swap + kernel tuning for the 4 GB RAM / eMMC combo
- A point-and-click installer — no terminal needed to install
- Optional local AI assistant (`tools/setup-ai.sh`, sized for this hardware)

## Getting the ISO

**Option A — GitHub Actions (no Linux machine needed):**
go to the repo's **Actions** tab → **Build ACE OS ISO** → **Run workflow**.
When it finishes (~30–60 min), download the `ace-os-iso` artifact and unzip
it to get the `.iso` file.

**Option B — build locally** (any machine with Docker):

```sh
./build.sh
```

Or on a Debian system without Docker: `sudo ./build.sh --native`
(requires `apt install live-build`).

Either way you end up with `ace-os-<date>-amd64.iso` (~2 GB).

## Installing on the laptop

Follow the step-by-step guide — it covers flashing the USB stick, the ASUS
BIOS keys (`F2` / `Esc`), Secure Boot, and the installer:

**[docs/install-asus-l510m.md](docs/install-asus-l510m.md)**

Hardware details and tuning rationale: [docs/hardware-l510m.md](docs/hardware-l510m.md)

## Repository layout

```
build.sh                 One-command ISO build (Docker or native Debian)
config/                  Debian live-build configuration
  auto/config            Distribution, architecture, ISO metadata
  package-lists/         What gets installed on the ISO
  includes.chroot/       Files shipped into the OS (zram config, sysctl, wallpaper)
  hooks/normal/          Build-time customization (branding)
tools/setup-ai.sh        Optional post-install local AI (Ollama + small model)
docs/                    Install guide + hardware notes for the L510M
.github/workflows/       CI build of the ISO
```

## License

See [LICENSE](LICENSE).
