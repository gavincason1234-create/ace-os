# Installing ACE OS on the ASUS VivoBook L510M (L510MA-WSQ5)

This guide takes you from a working Windows laptop to a laptop running ACE OS.

> **Warning — this erases the laptop.** The install wipes Windows and
> everything on the machine. This laptop is a fresh start, so that's fine —
> but if there is anything on it you care about, copy it to a USB drive or
> cloud storage first.

## What you need

- The ASUS L510M, plugged into its charger
- A USB flash drive, **8 GB or larger** (it will be erased)
- Another computer (or this one, while Windows still works) to flash the USB
- The ACE OS ISO file (see "Getting the ISO" in the main README)

## Step 1 — Flash the ISO to the USB drive

**Automatic (recommended):** if you installed the `ace-os` pip tool (see the
main README), plug in the USB drive and run `ace-os` — as Administrator on
Windows, or with `sudo` on Linux/macOS. It downloads the ISO and writes the
USB drive for you; then skip straight to Step 2.

**Manual:** on Windows, use [Rufus](https://rufus.ie) (or [balenaEtcher](https://etcher.balena.io)):

1. Open Rufus and plug in the USB drive.
2. **Device:** your USB drive. **Boot selection:** the `ace-os-....iso` file.
3. Leave everything else at the defaults and click **START**.
   If Rufus asks about ISO vs DD mode, pick **DD mode**.
4. Wait for it to finish, then close Rufus.

On Linux/Mac: `sudo dd if=ace-os-*.iso of=/dev/sdX bs=4M status=progress` (replace `sdX` with the USB device — double-check, dd erases whatever you point it at).

## Step 2 — Boot the laptop from the USB drive

1. Plug the USB drive into the laptop and shut it down fully.
2. Turn it on and immediately **tap the `F2` key repeatedly** — this opens
   the ASUS UEFI (BIOS) setup screen.
3. Go to the **Security** tab → **Secure Boot** → set **Secure Boot Control**
   to **Disabled**.
4. Go to the **Boot** tab and make sure **Fast Boot** is **Disabled**.
5. Press **F10** to save and exit. As it restarts, **tap `Esc`** to open the
   one-time boot menu and choose your USB drive (usually shown by its brand
   name, with "UEFI" in front).
6. At the ACE OS menu, press Enter on the first entry. After a minute you'll
   be at the ACE OS desktop — this is a live session running from the USB,
   nothing is installed yet. Feel free to try it out.

If the laptop just boots back into Windows, redo steps 2–5 — the boot menu
(`Esc`) step is the one that matters.

## Step 3 — Install ACE OS

1. Connect to Wi-Fi using the network icon in the top-right corner.
2. Double-click the **Install** icon on the desktop (the installer is
   Debian's Calamares — ACE OS is built on Debian).
3. Pick your language, region, and keyboard layout.
4. At the partitioning step choose **Erase disk**. Make sure the target is
   the internal **128 GB eMMC** drive (about 115 GiB, named `mmcblk0`) —
   **not** the USB stick. Enable the **swap (with hibernate)** or
   **swap (no hibernate)** option if offered.
5. Create your user account and a password you'll remember.
6. Review the summary, click **Install**, and wait (15–30 minutes on this
   machine).
7. When it finishes, choose **Restart now** and unplug the USB drive when
   the screen goes dark.

## Step 4 — First boot checklist

After the reboot you're in your installed ACE OS. Do this once:

1. Connect to Wi-Fi.
2. Open a terminal (Applications → Terminal Emulator) and update:

   ```sh
   sudo apt update && sudo apt full-upgrade -y
   ```

3. Check that things work: Wi-Fi, sound (play a video in Firefox),
   brightness keys (`F4`/`F5`), and suspend (close and reopen the lid).

## Optional — local AI assistant

With ACE OS installed and online, you can add a small local AI model:

```sh
sh tools/setup-ai.sh
ollama run qwen2.5:1.5b
```

Be realistic: on this laptop's Celeron CPU and 4 GB RAM, small models answer
slowly (a few words per second) — fine for quick questions. For serious AI
help, use a cloud assistant like [claude.ai](https://claude.ai) in Firefox.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| USB won't boot | Confirm Secure Boot is disabled (Step 2.3) and use the `Esc` boot menu. Try the other USB port. Re-flash with Rufus in DD mode. |
| No Wi-Fi in the live session or after install | The Intel wireless firmware ships on the ISO; run `sudo dmesg \| grep -i firmware` and see `docs/hardware-l510m.md`. |
| No sound | Check the output device in the volume icon → Audio mixer; see the sound notes in `docs/hardware-l510m.md`. |
| Installer doesn't see the 128 GB disk | The eMMC appears as `mmcblk0`, not `sda`. If truly absent, check the SATA/storage mode in the UEFI setup is the default. |
| Feels slow with many tabs | Normal for 4 GB RAM. ACE OS ships zram compressed swap to soften this; keep open tabs/apps modest. |
