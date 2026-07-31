# ACE OS

## 🟦 [👉 CLICK HERE TO START — simple step-by-step instructions](START-HERE.md)

*(Everything below is extra detail — the link above is all you need.)*

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

## The easiest way: one file, no pip

If `pip` isn't working on your machine, you only need Python itself
(Windows: type `python` in Command Prompt and install it from the Microsoft
Store that opens; then open a NEW Command Prompt **as Administrator**).
Then paste these two lines:

```
curl -L -o get-ace-os.py https://raw.githubusercontent.com/gavincason1234-create/ace-os/main/get-ace-os.py
py get-ace-os.py
```

(Linux/macOS: `sudo python3 get-ace-os.py`.) The script downloads the latest
ACE OS ISO, verifies it, and writes it to a USB drive you pick — same safety
rules as the pip tool: USB drives only, and you must type `ERASE`.

No Python at all? Plan B needs only a browser: download
[the ISO directly](https://github.com/gavincason1234-create/ace-os/releases/latest/download/ace-os-amd64.iso)
and flash it with [Rufus](https://rufus.ie) (portable version, run as
administrator, pick the ISO, START).

## The easy way: pip

One command downloads the ISO and makes the bootable USB stick for you.
On any computer with Python installed (on Windows, get it from
[python.org](https://python.org) and tick "Add python.exe to PATH"):

```sh
pip install https://github.com/gavincason1234-create/ace-os/releases/latest/download/aceos_installer-1.0.0-py3-none-any.whl
ace-os
```

Plug in a USB stick (8 GB+, it will be erased), and `ace-os` does the rest:
downloads the latest ACE OS ISO, verifies it, asks which USB drive to use,
and writes it. On Windows run the terminal **as Administrator**; on
Linux/macOS run `ace-os` with `sudo`.

> One-time setup first: the ISO has to exist before it can be downloaded —
> go to the repo's **Actions** tab → **Build ACE OS ISO** → **Run workflow**
> once (~30–60 min). The workflow publishes the ISO and the pip package to
> the repo's **Releases** page automatically. For the pip/download links to
> work without a login, the repo must be **public** (or set an
> `ACEOS_GITHUB_TOKEN` environment variable with repo read access).

## Other ways to get the ISO

**GitHub Actions artifact:** every workflow run also attaches the ISO as a
downloadable artifact (Actions tab → the run → `ace-os-iso`).

**Build locally** (any machine with Docker):

```sh
./build.sh
```

Or on a Debian system without Docker: `sudo ./build.sh --native`
(requires `apt install live-build`).

Either way you end up with an `.iso` (~2 GB) to flash with Rufus/Etcher —
or with `ace-os flash --iso path/to/file.iso`.

## Installing on the laptop

Follow the step-by-step guide — it covers flashing the USB stick, the ASUS
BIOS keys (`F2` / `Esc`), Secure Boot, and the installer:

**[docs/install-asus-l510m.md](docs/install-asus-l510m.md)**

Hardware details and tuning rationale: [docs/hardware-l510m.md](docs/hardware-l510m.md)

## Repository layout

```
build.sh                 One-command ISO build (Docker or native Debian)
auto/                    live-build entry points (config: distribution, arch, ISO metadata)
config/                  Debian live-build configuration
  package-lists/         What gets installed on the ISO
  includes.chroot/       Files shipped into the OS (zram config, sysctl, wallpaper)
  hooks/normal/          Build-time customization (branding)
installer/               pip package (aceos-installer): the `ace-os` command that
                         downloads the ISO and writes the bootable USB
tools/setup-ai.sh        Optional post-install local AI (Ollama + small model)
docs/                    Install guide + hardware notes for the L510M
.github/workflows/       CI: builds the ISO + pip wheel, publishes them as a release
```

## License

See [LICENSE](LICENSE).
