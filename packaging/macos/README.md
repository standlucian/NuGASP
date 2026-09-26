# NuTrackN - macOS Packaging Guide

This directory contains the automated packaging toolchain to produce native **Application Bundles (`NuTrackN.app`)** and **Drag-and-Drop Disk Images (`.dmg`)** for macOS.

---

## 🏗️ Architecture Support
The packaging scripts automatically detect the host architecture (`uname -m`):
- **Apple Silicon (M1/M2/M3/M4):** Produces `dist/NuTrackN-macOS-arm64.dmg`
- **Intel Macs (x86_64):** Produces `dist/NuTrackN-macOS-x86_64.dmg`

Both packages are completely self-contained with bundled Qt5 frameworks, CERN ROOT dynamic libraries, Cling interpreter, and module headers.

---

## 🚀 Automated Builds via GitHub Actions (Apple Silicon ARM64)

The repository includes a GitHub Actions workflow that automatically builds Apple Silicon packages:
- Go to the **Actions** tab on your GitHub repository.
- Select **Build & Package macOS (Apple Silicon ARM64)** and click **Run workflow**.
- GitHub will spin up an Apple Silicon Mac runner, compile, and make **`NuTrackN-macOS-arm64.dmg`** available for download under Artifacts or Releases in ~15 minutes.

For older Intel Macs (x86_64), build directly on a physical Intel machine using the local build steps below.

---

## 💻 Local Build on a Mac

If you are running on a Mac with Homebrew installed:

### 1. Install Prerequisites
```bash
brew install qt@5 root
pip3 install Pillow
```

### 2. Run the Packager
```bash
./packaging/macos/build_dmg.sh
```

The resulting disk image will be placed in `dist/`.

---

## ⚡ 1-Click Terminal Shortcut (`nutrackn`) on macOS

When a user has installed `NuTrackN.app` in `/Applications`, they can easily add the `nutrackn` command to their terminal:

```bash
/Applications/NuTrackN.app/Contents/MacOS/nutrackn --install-cli
```
This creates a symlink in `/usr/local/bin/nutrackn` (or `~/.local/bin/nutrackn`) so they can launch it directly from any Terminal session just like `xtrackn`.
