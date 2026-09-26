#!/usr/bin/env bash
# ==============================================================================
# NuGASP / NuTrackN - 1-Click Linux AppImage Packaging Script
# Bundles NuTrackN, Qt5 runtimes, and CERN ROOT libraries into a single portable
# standalone AppImage executable.
# ==============================================================================

set -eo pipefail

# Script directory and workspace root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NUTRACKN_DIR="${ROOT_DIR}/NuTrackN"
BUILD_DIR="${SCRIPT_DIR}/build"
APPDIR="${BUILD_DIR}/AppDir"
DIST_DIR="${ROOT_DIR}/dist"
CACHE_DIR="${SCRIPT_DIR}/.cache"

APP_NAME="NuTrackN"
APP_EXE="nutrackn"
APPIMAGE_OUTPUT="${DIST_DIR}/${APP_NAME}-x86_64.AppImage"

echo "======================================================================"
echo "    🚀  Building 1-Click Linux AppImage for ${APP_NAME}               "
echo "======================================================================"

# ------------------------------------------------------------------------------
# 1. Check prerequisites
# ------------------------------------------------------------------------------
echo "==> [1/6] Checking build prerequisites..."

if ! command -v qmake >/dev/null 2>&1 && ! command -v qt5-qmake >/dev/null 2>&1; then
    echo "[-] Error: Qt5 qmake was not found in PATH." >&2
    exit 1
fi
QMAKE_BIN="$(command -v qmake || command -v qt5-qmake)"
QT_PLUGINS_DIR="$("${QMAKE_BIN}" -query QT_INSTALL_PLUGINS)"
QT_LIBS_DIR="$("${QMAKE_BIN}" -query QT_INSTALL_LIBS)"

# Detect ROOT
if [ -z "${ROOTSYS}" ]; then
    if command -v root-config >/dev/null 2>&1; then
        ROOTSYS="$(root-config --prefix)"
    elif [ -d "/home/lucian/root" ]; then
        ROOTSYS="/home/lucian/root"
    elif [ -d "/opt/root" ]; then
        ROOTSYS="/opt/root"
    fi
fi

if [ -z "${ROOTSYS}" ] || [ ! -d "${ROOTSYS}" ]; then
    echo "[-] Error: CERN ROOT (ROOTSYS) could not be located." >&2
    echo "    Please source your thisroot.sh (e.g. 'source /path/to/root/bin/thisroot.sh') and re-run." >&2
    exit 1
fi
echo "    ✓ Found Qt5 installation: ${QT_LIBS_DIR}"
echo "    ✓ Found CERN ROOT: ${ROOTSYS}"

# ------------------------------------------------------------------------------
# 2. Compile NuTrackN binary
# ------------------------------------------------------------------------------
echo "==> [2/6] Compiling ${APP_NAME} binary..."
cd "${NUTRACKN_DIR}"
if [ ! -f "Makefile" ]; then
    "${QMAKE_BIN}" nutrackn.pro
fi
make -j"$(nproc)"

if [ ! -f "${NUTRACKN_DIR}/${APP_EXE}" ]; then
    echo "[-] Error: Compilation failed. Binary '${APP_EXE}' was not found." >&2
    exit 1
fi
echo "    ✓ Compiled binary: ${NUTRACKN_DIR}/${APP_EXE}"

# ------------------------------------------------------------------------------
# 3. Assemble AppDir structure
# ------------------------------------------------------------------------------
echo "==> [3/6] Assembling AppDir structure..."
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/plugins"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
mkdir -p "${DIST_DIR}"
mkdir -p "${CACHE_DIR}"

# Copy binary
cp -a "${NUTRACKN_DIR}/${APP_EXE}" "${APPDIR}/usr/bin/${APP_EXE}"

# Copy icons & desktop metadata
cp -a "${SCRIPT_DIR}/nutrackn.desktop" "${APPDIR}/nutrackn.desktop"
cp -a "${SCRIPT_DIR}/nutrackn.desktop" "${APPDIR}/usr/share/applications/${APP_EXE}.desktop"

if [ -f "${NUTRACKN_DIR}/c2picon.png" ]; then
    cp -a "${NUTRACKN_DIR}/c2picon.png" "${APPDIR}/nutrackn.png"
    cp -a "${NUTRACKN_DIR}/c2picon.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/${APP_EXE}.png"
elif [ -f "${NUTRACKN_DIR}/icon.png" ]; then
    cp -a "${NUTRACKN_DIR}/icon.png" "${APPDIR}/nutrackn.png"
    cp -a "${NUTRACKN_DIR}/icon.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/${APP_EXE}.png"
fi

# Copy ROOT runtime data (etc/, fonts/)
if [ -d "${ROOTSYS}/etc" ]; then
    mkdir -p "${APPDIR}/usr/etc"
    cp -a "${ROOTSYS}/etc" "${APPDIR}/usr/"
fi
if [ -d "${ROOTSYS}/fonts" ]; then
    mkdir -p "${APPDIR}/usr/fonts"
    cp -a "${ROOTSYS}/fonts" "${APPDIR}/usr/"
fi

# ------------------------------------------------------------------------------
# 4. Bundle Shared Libraries & Qt Plugins
# ------------------------------------------------------------------------------
echo "==> [4/6] Bundling Qt5 and CERN ROOT runtime libraries..."

# Copy Qt plugins
for plugin in platforms xcbglintegrations imageformats iconengines styles platformthemes; do
    if [ -d "${QT_PLUGINS_DIR}/${plugin}" ]; then
        cp -a "${QT_PLUGINS_DIR}/${plugin}" "${APPDIR}/usr/plugins/"
    fi
done

# Collect all dynamic library dependencies
collect_libs() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '/=>/ { print $3 }' | while read -r lib; do
        if [ -n "$lib" ] && [ -f "$lib" ]; then
            local base
            base="$(basename "$lib")"
            # Exclude core libc/glibc/driver system libraries to maximize portability across distributions
            case "$base" in
                ld-linux*.so*|libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|libresolv.so*|libutil.so*)
                    continue
                    ;;
                libGL.so*|libGLX.so*|libGLdispatch.so*|libEGL.so*|libdrm.so*|libasound.so*|libpulse.so*)
                    continue
                    ;;
                libX11.so*|libxcb.so*|libXau.so*|libXdmcp.so*|libXext.so*)
                    continue
                    ;;
                *)
                    if [ ! -f "${APPDIR}/usr/lib/${base}" ]; then
                        cp -aL "$lib" "${APPDIR}/usr/lib/"
                    fi
                    ;;
            esac
        fi
    done
}

# Scan binary and plugins for dependencies
collect_libs "${APPDIR}/usr/bin/${APP_EXE}"
find "${APPDIR}/usr/plugins" -type f -name "*.so*" -exec ldd {} 2>/dev/null \; | awk '/=>/ { print $3 }' | while read -r lib; do
    if [ -n "$lib" ] && [ -f "$lib" ]; then
        collect_libs "$lib"
    fi
done

# Ensure all essential ROOT libraries and dictionaries are copied
if [ -d "${ROOTSYS}/lib" ]; then
    for rlib in libCore.so* libHist.so* libGraf.so* libGraf3d.so* libGpad.so* libMatrix.so* libMathCore.so* libSpectrum.so* libRIO.so* libThread.so* libImt.so* libNet.so* libMultiProc.so* libTree.so* libRint.so* libPhysics.so* libPostscript.so* libGui.so* *.pcm; do
        for f in "${ROOTSYS}/lib/"${rlib}; do
            if [ -f "$f" ]; then
                cp -aL "$f" "${APPDIR}/usr/lib/"
            fi
        done
    done
fi

# Create AppRun launcher
cat << 'EOF' > "${APPDIR}/AppRun"
#!/bin/bash
set -e

# Resolve AppDir absolute path
SELF="$(readlink -f "$0")"
APPDIR="$(dirname "$SELF")"

export APPDIR="${APPDIR}"
export PATH="${APPDIR}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${APPDIR}/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${APPDIR}/usr/plugins/platforms"

# Configure embedded ROOT runtime
export ROOTSYS="${APPDIR}/usr"
export ROOT_CONFIG_SEARCH_PATH="${APPDIR}/usr/etc/root"
if [ -d "${APPDIR}/usr/etc/root" ]; then
    export ROOTRC="${APPDIR}/usr/etc/root/system.rootrc"
fi
if [ -d "${APPDIR}/usr/fonts" ]; then
    export ROOT_TTFONTS="${APPDIR}/usr/fonts"
fi

exec "${APPDIR}/usr/bin/nutrackn" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# ------------------------------------------------------------------------------
# 5. Acquire appimagetool and package
# ------------------------------------------------------------------------------
echo "==> [5/6] Generating AppImage bundle..."

APPIMAGETOOL=""
if command -v appimagetool >/dev/null 2>&1; then
    APPIMAGETOOL="$(command -v appimagetool)"
elif [ -f "${CACHE_DIR}/appimagetool-x86_64.AppImage" ]; then
    APPIMAGETOOL="${CACHE_DIR}/appimagetool-x86_64.AppImage"
else
    echo "    Downloading appimagetool utility..."
    APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    curl -sSL -o "${CACHE_DIR}/appimagetool-x86_64.AppImage" "${APPIMAGETOOL_URL}" || \
    wget -q -O "${CACHE_DIR}/appimagetool-x86_64.AppImage" "${APPIMAGETOOL_URL}"
    chmod +x "${CACHE_DIR}/appimagetool-x86_64.AppImage"
    APPIMAGETOOL="${CACHE_DIR}/appimagetool-x86_64.AppImage"
fi

# Run appimagetool
export ARCH="x86_64"
echo "    Executing: appimagetool ..."
ARCH=x86_64 "${APPIMAGETOOL}" --appimage-extract-and-run "${APPDIR}" "${APPIMAGE_OUTPUT}" 2>&1 || \
ARCH=x86_64 "${APPIMAGETOOL}" "${APPDIR}" "${APPIMAGE_OUTPUT}"

chmod +x "${APPIMAGE_OUTPUT}"

# ------------------------------------------------------------------------------
# 6. Summary
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "    🎉 AppImage built successfully!                                  "
echo "======================================================================"
echo "    File: $(ls -lh "${APPIMAGE_OUTPUT}" | awk '{print $9, "(" $5 ")"}')"
echo ""
echo "    To test and run directly on any Linux machine:"
echo "      ${APPIMAGE_OUTPUT}"
echo "======================================================================"
