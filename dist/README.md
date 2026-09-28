# NuTrackN - 1-Click Linux Distribution Guide

This directory contains standalone, self-contained packages for **NuTrackN (NuGASP)**. 
All required dependencies (including **CERN ROOT** and **Qt5**) are bundled inside the AppImage. No external installations or root/administrator privileges are required.

---

## Quick Start (Run Instantly)

Make the AppImage executable (if needed) and run it:

```bash
chmod +x NuTrackN-x86_64.AppImage
./NuTrackN-x86_64.AppImage
```

You can also pass spectrum files directly as arguments:
```bash
./NuTrackN-x86_64.AppImage my_spectrum.spe
```

### ⚠️ Troubleshooting: "open dir error" (Missing FUSE on Ubuntu 22.04 / 24.04)
If you run the AppImage on another computer and see:
```text
open dir error: No such file or directory
Cannot mount AppImage, please check your FUSE setup.
```
This is because modern Linux distributions (such as Ubuntu 22.04+, Ubuntu 24.04, Debian 12+) do not include `libfuse2` by default.

**You have three easy ways to resolve this:**

1. **Run with `--appimage-extract-and-run` (No root/install needed):**
   ```bash
   ./NuTrackN-x86_64.AppImage --appimage-extract-and-run
   ```
   Or set the environment variable:
   ```bash
   APPIMAGE_EXTRACT_AND_RUN=1 ./NuTrackN-x86_64.AppImage
   ```

2. **Or install the FUSE library on the host system:**
   - **Ubuntu 22.04 / Debian 11/12:** `sudo apt install libfuse2`
   - **Ubuntu 24.04+:** `sudo apt install libfuse2t64`
   - **Fedora:** `sudo dnf install fuse fuse-libs`
   - **Arch:** `sudo pacman -S fuse2`

3. **Or use the standalone portable tarball (100% FUSE-Free):**
   ```bash
   tar -xzf NuTrackN-linux-x86_64-portable.tar.gz
   ./NuTrackN/AppRun
   ```


---

## 1-Click System & Terminal Integration (`nutrackn`)

To use `nutrackn` like a standard command line tool (just like you call `xtrackn`), run:

```bash
./NuTrackN-x86_64.AppImage --install
```
*(or `./NuTrackN-x86_64.AppImage -i`)*

### What `--install` does:
1. **Terminal Command**: Installs `nutrackn` into `~/.local/bin/nutrackn` so you can launch it simply by typing:
   ```bash
   nutrackn
   ```
   from any terminal in any directory.
2. **Desktop Launcher**: Creates an application entry in your system's desktop menu (`~/.local/share/applications/nutrackn.desktop`).
3. **High-Res Icon**: Installs high-resolution icons into your system's icon theme.
4. **PATH Configuration**: Automatically ensures `~/.local/bin` is in your shell `$PATH` (in `~/.bashrc` / `~/.zshrc`).

---

## 1-Click Uninstallation

To cleanly remove the `nutrackn` terminal command, desktop menu launcher, and icons from your system:

```bash
./NuTrackN-x86_64.AppImage --uninstall
```
*(or `./NuTrackN-x86_64.AppImage -u`, or `nutrackn --uninstall`)*

---

## Command Options Summary

| Command Option | Description |
| :--- | :--- |
| `nutrackn` | Launches the interactive GUI application |
| `nutrackn [file ...]` | Launches the GUI and loads specified spectrum file(s) |
| `./NuTrackN-x86_64.AppImage --install` (or `-i`) | 1-click install of terminal shortcut and desktop launcher |
| `./NuTrackN-x86_64.AppImage --uninstall` (or `-u`)| Cleanly removes terminal command and desktop shortcut |
| `./NuTrackN-x86_64.AppImage --help` (or `-h`) | Displays CLI help and integration instructions |

---

## Rebuilding the AppImage
To rebuild the AppImage with any new code updates:
```bash
./packaging/linux/build_appimage.sh
```
