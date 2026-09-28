# NuTrackN - 1-Click Linux Distribution Guide

This directory contains standalone, self-contained packages for **NuTrackN (NuGASP)**. 
All required dependencies (including **CERN ROOT** and **Qt5**) are bundled inside the AppImage. No external installations or root/administrator privileges are required.

---

## Quick Start (1-Click Workflow)

On **any** Linux distribution (Ubuntu 20.04+, 22.04+, 24.04+, Debian, Fedora, Arch, Linux Mint, etc.):

```bash
chmod +x NuTrackN-x86_64.AppImage
./NuTrackN-x86_64.AppImage --install
```

**All is ready!** 
No installing FUSE (`libfuse2`), no manual unpacking, and no administrator (`sudo`) privileges required.

You can now immediately launch NuTrackN:
- **From your Desktop**: Double-click **NuTrackN** on your desktop.
- **From Applications menu**: Open **NuTrackN** (under *Science* or *Education*).
- **From any terminal**:
  ```bash
  nutrackn
  # or with a spectrum file:
  nutrackn my_spectrum.spe
  ```
  *(If using the exact same terminal where you just ran `--install`, run `source ~/.profile` or open a new terminal window).*

---

## Direct Portable Execution (Without Installing)

If you just want to run the AppImage without installing system shortcuts:
```bash
chmod +x NuTrackN-x86_64.AppImage
./NuTrackN-x86_64.AppImage [spectrum_files...]
```
On first launch, it will prepare the environment in `~/.cache/nutrackn/` (~4s). Subsequent launches start immediately in under 0.1s.


---

## 1-Click System & Terminal Integration (`nutrackn`)

To use `nutrackn` like a standard command line tool (just like you call `xtrackn`), run:

```bash
./NuTrackN-x86_64.AppImage --install
```
*(or `./NuTrackN-x86_64.AppImage -i`)*

### What `--install` does:
1. **Desktop Shortcut**: Creates a trusted double-clickable application launcher on your actual Desktop (`~/Desktop/NuTrackN.desktop`).
2. **Terminal Command**: Installs `nutrackn` into `~/.local/bin/nutrackn` and `~/bin/nutrackn`.
3. **Application Menu**: Adds **NuTrackN** to your system's desktop menu (`~/.local/share/applications/nutrackn.desktop`).
4. **High-Res Icon**: Installs high-resolution icons (512x512 down to 16x16) into your icon theme.
5. **PATH Configuration**: Ensures `~/.local/bin` and `~/bin` are configured across shell profiles (`~/.bashrc`, `~/.profile`, `~/.zshrc`).

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
