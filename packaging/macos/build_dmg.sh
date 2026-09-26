#!/usr/bin/env bash
# ==============================================================================
# NuGASP / NuTrackN - 1-Click macOS App Bundle & DMG Packaging Script
# Automatically detects Apple Silicon (arm64) vs Intel (x86_64) architecture,
# compiles native binary, bundles Qt5 and CERN ROOT runtimes, and generates
# a drag-and-drop .dmg disk image.
# ==============================================================================

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NUTRACKN_DIR="${ROOT_DIR}/NuTrackN"
DIST_DIR="${ROOT_DIR}/dist"
BUILD_DIR="${SCRIPT_DIR}/build"
APP_BUNDLE="${BUILD_DIR}/NuTrackN.app"

# Detect hardware architecture
ARCH="$(uname -m)"
echo "======================================================================"
echo "    🍏 Building 1-Click macOS Standalone DMG for NuTrackN             "
echo "    Target Architecture: ${ARCH}                                      "
echo "======================================================================"

# ------------------------------------------------------------------------------
# 1. Detect Prerequisites (Homebrew, Qt5, CERN ROOT)
# ------------------------------------------------------------------------------
echo "==> [1/6] Detecting toolchains and prerequisites..."

if [ -f "/opt/homebrew/bin/brew" ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
elif [ -f "/usr/local/bin/brew" ]; then
    eval "$(/usr/local/bin/brew shellenv)"
fi

# Detect Qt5
QT5_PREFIX=""
if [ -n "${QTDIR}" ] && [ -d "${QTDIR}" ]; then
    QT5_PREFIX="${QTDIR}"
elif command -v brew >/dev/null 2>&1; then
    QT5_PREFIX="$(brew --prefix qt@5 2>/dev/null || brew --prefix qt 2>/dev/null || true)"
fi

if [ -z "${QT5_PREFIX}" ] || [ ! -d "${QT5_PREFIX}" ]; then
    echo "[-] Error: Qt5 installation could not be located." >&2
    echo "    Please install it via Homebrew: brew install qt@5" >&2
    exit 1
fi
export PATH="${QT5_PREFIX}/bin:${PATH}"
QMAKE_BIN="$(command -v qmake)"
MACDEPLOYQT_BIN="${QT5_PREFIX}/bin/macdeployqt"

# Detect CERN ROOT
if [ -z "${ROOTSYS}" ]; then
    if command -v root-config >/dev/null 2>&1; then
        ROOTSYS="$(root-config --prefix)"
    elif command -v brew >/dev/null 2>&1; then
        ROOTSYS="$(brew --prefix root 2>/dev/null || true)"
    fi
fi

if [ -z "${ROOTSYS}" ] || [ ! -d "${ROOTSYS}" ]; then
    echo "[-] Error: CERN ROOT (ROOTSYS) could not be located." >&2
    echo "    Please install ROOT or source thisroot.sh before running." >&2
    exit 1
fi
export ROOTSYS
if [ -f "${ROOTSYS}/bin/thisroot.sh" ]; then
    source "${ROOTSYS}/bin/thisroot.sh"
fi
export PATH="${ROOTSYS}/bin:${PATH}"

echo "    ✓ Architecture: ${ARCH}"
echo "    ✓ Found Qt5 at: ${QT5_PREFIX}"
echo "    ✓ Found CERN ROOT at: ${ROOTSYS}"

# ------------------------------------------------------------------------------
# 2. Compile NuTrackN Binary
# ------------------------------------------------------------------------------
echo "==> [2/6] Compiling NuTrackN binary (${ARCH})..."
cd "${NUTRACKN_DIR}"
"${QMAKE_BIN}" nutrackn.pro -spec macx-clang
make clean && make -j"$(sysctl -n hw.ncpu || echo 4)"

if [ ! -f "${NUTRACKN_DIR}/nutrackn" ]; then
    echo "[-] Error: Compilation failed. Binary not found." >&2
    exit 1
fi
echo "    ✓ Native compilation successful."

# ------------------------------------------------------------------------------
# 3. Assemble NuTrackN.app Bundle Structure
# ------------------------------------------------------------------------------
echo "==> [3/6] Assembling native NuTrackN.app bundle..."
rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources/root"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${APP_BUNDLE}/Contents/PlugIns"
mkdir -p "${DIST_DIR}"

# Copy binary and launcher
cp -a "${NUTRACKN_DIR}/nutrackn" "${APP_BUNDLE}/Contents/MacOS/nutrackn_bin"
cp -a "${SCRIPT_DIR}/nutrackn_launcher" "${APP_BUNDLE}/Contents/MacOS/nutrackn"
chmod +x "${APP_BUNDLE}/Contents/MacOS/nutrackn" "${APP_BUNDLE}/Contents/MacOS/nutrackn_bin"

# Copy Info.plist
cp -a "${SCRIPT_DIR}/Info.plist" "${APP_BUNDLE}/Contents/Info.plist"

# Copy / Generate Apple ICNS
if [ ! -f "${SCRIPT_DIR}/NuTrackN.icns" ]; then
    python3 "${SCRIPT_DIR}/generate_icns.py" || true
fi
if [ -f "${SCRIPT_DIR}/NuTrackN.icns" ]; then
    cp -a "${SCRIPT_DIR}/NuTrackN.icns" "${APP_BUNDLE}/Contents/Resources/NuTrackN.icns"
fi

# Copy ROOT runtime data (etc/, fonts/, include/)
if [ -d "${ROOTSYS}/etc" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/etc"
    cp -a "${ROOTSYS}/etc" "${APP_BUNDLE}/Contents/Resources/root/"
fi
if [ -d "${ROOTSYS}/fonts" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/fonts"
    cp -a "${ROOTSYS}/fonts" "${APP_BUNDLE}/Contents/Resources/root/"
fi
if [ -d "${ROOTSYS}/include" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/include"
    cp -a "${ROOTSYS}/include" "${APP_BUNDLE}/Contents/Resources/root/"
fi

# ------------------------------------------------------------------------------
# 4. Deploy Qt Frameworks & ROOT Dynamic Libraries
# ------------------------------------------------------------------------------
echo "==> [4/6] Deploying Qt5 frameworks & CERN ROOT dynamic libraries..."

# Run macdeployqt
if [ -x "${MACDEPLOYQT_BIN}" ]; then
    echo "    Running macdeployqt..."
    "${MACDEPLOYQT_BIN}" "${APP_BUNDLE}" -always-overwrite 2>&1 || true
fi

# Copy all CERN ROOT dynamic libraries & PCMs
if [ -d "${ROOTSYS}/lib" ]; then
    echo "    Bundling ROOT dylibs and Cling dictionaries..."
    cp -a "${ROOTSYS}/lib/"lib*.dylib "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
    cp -a "${ROOTSYS}/lib/"lib*.so* "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
    cp -a "${ROOTSYS}/lib/"*.pcm "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
fi

# Fix dynamic linker load paths (@rpath)
echo "    Configuring Mach-O @rpath library references..."
install_name_tool -add_rpath "@executable_path/../Frameworks" "${APP_BUNDLE}/Contents/MacOS/nutrackn_bin" 2>/dev/null || true

# Point all bundled ROOT dylibs to @rpath
for dylib in "${APP_BUNDLE}/Contents/Frameworks/"*.dylib; do
    if [ -f "$dylib" ]; then
        install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib" 2>/dev/null || true
    fi
done

# ------------------------------------------------------------------------------
# 5. Create Drag-and-Drop DMG Disk Image
# ------------------------------------------------------------------------------
echo "==> [5/6] Generating drag-and-drop .dmg disk image..."

DMG_NAME="NuTrackN-macOS-${ARCH}.dmg"
OUTPUT_DMG="${DIST_DIR}/${DMG_NAME}"
DMG_STAGE="${BUILD_DIR}/dmg_stage"

rm -rf "${DMG_STAGE}" "${OUTPUT_DMG}"
mkdir -p "${DMG_STAGE}"

# Copy App bundle and create /Applications shortcut
cp -a "${APP_BUNDLE}" "${DMG_STAGE}/NuTrackN.app"
ln -s "/Applications" "${DMG_STAGE}/Applications"

# Create disk image
hdiutil create \
    -volname "NuTrackN" \
    -srcfolder "${DMG_STAGE}" \
    -ov \
    -format UDZO \
    "${OUTPUT_DMG}"

# ------------------------------------------------------------------------------
# 6. Summary
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "    🎉 macOS DMG Built Successfully!                                  "
echo "======================================================================"
echo "    Output File: ${OUTPUT_DMG}"
echo "    Architecture: ${ARCH}"
echo ""
echo "    Users can double-click the DMG and drag NuTrackN.app into Applications!"
echo "======================================================================"
