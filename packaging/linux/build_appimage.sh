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
    elif [ -d "${HOME}/root" ]; then
        ROOTSYS="${HOME}/root"
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

# Copy multi-resolution icons
if [ -f "${NUTRACKN_DIR}/nutrackn.png" ]; then
    cp -a "${NUTRACKN_DIR}/nutrackn.png" "${APPDIR}/nutrackn.png"
    cp -a "${NUTRACKN_DIR}/nutrackn.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/${APP_EXE}.png"
elif [ -f "${NUTRACKN_DIR}/icon.png" ]; then
    cp -a "${NUTRACKN_DIR}/icon.png" "${APPDIR}/nutrackn.png"
    cp -a "${NUTRACKN_DIR}/icon.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/${APP_EXE}.png"
fi

for s in 16 32 48 64 128 256; do
    if [ -f "${NUTRACKN_DIR}/nutrackn_${s}.png" ]; then
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps"
        cp -a "${NUTRACKN_DIR}/nutrackn_${s}.png" "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps/${APP_EXE}.png"
    fi
done

# Copy ROOT runtime data (etc/, fonts/, include/)
if [ -d "${ROOTSYS}/etc" ]; then
    mkdir -p "${APPDIR}/usr/etc"
    cp -a "${ROOTSYS}/etc" "${APPDIR}/usr/"
fi
if [ -d "${ROOTSYS}/fonts" ]; then
    mkdir -p "${APPDIR}/usr/fonts"
    cp -a "${ROOTSYS}/fonts" "${APPDIR}/usr/"
fi
if [ -d "${ROOTSYS}/include" ]; then
    mkdir -p "${APPDIR}/usr/include"
    cp -a "${ROOTSYS}/include" "${APPDIR}/usr/"
fi

# Copy documentation
if [ -d "${ROOT_DIR}/docs" ]; then
    mkdir -p "${APPDIR}/usr/share/doc/nutrackn"
    cp -a "${ROOT_DIR}/docs/"* "${APPDIR}/usr/share/doc/nutrackn/"
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

# Ensure all ROOT libraries, runtime plugins, Cling, and dictionaries are copied
if [ -d "${ROOTSYS}/lib" ]; then
    echo "    Bundling CERN ROOT libraries and Cling interpreter..."
    cp -a "${ROOTSYS}/lib/"lib*.so* "${APPDIR}/usr/lib/" 2>/dev/null || true
    cp -a "${ROOTSYS}/lib/"*.pcm "${APPDIR}/usr/lib/" 2>/dev/null || true
fi

# Bundle backward-compatibility glibc runtime to support older distributions (e.g. Ubuntu 22.04 with glibc 2.35)
echo "    Bundling backward-compatibility glibc runtime..."
mkdir -p "${APPDIR}/usr/lib/compat"
for glibc_lib in /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
                 /lib/x86_64-linux-gnu/libc.so.6 \
                 /lib/x86_64-linux-gnu/libm.so.6 \
                 /lib/x86_64-linux-gnu/libpthread.so.0 \
                 /lib/x86_64-linux-gnu/libdl.so.2 \
                 /lib/x86_64-linux-gnu/librt.so.1 \
                 /lib/x86_64-linux-gnu/libresolv.so.2 \
                 /lib/x86_64-linux-gnu/libutil.so.1; do
    if [ -f "$glibc_lib" ]; then
        cp -aL "$glibc_lib" "${APPDIR}/usr/lib/compat/"
    fi
done

# Create AppRun launcher with smart 1-click terminal & desktop integration
cat << 'EOF' > "${APPDIR}/AppRun"
#!/usr/bin/env bash
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
export ROOT_INCLUDE_PATH="${APPDIR}/usr/include"
if [ -d "${APPDIR}/usr/etc/root" ]; then
    export ROOTRC="${APPDIR}/usr/etc/root/system.rootrc"
fi
if [ -d "${APPDIR}/usr/fonts" ]; then
    export ROOT_TTFONTS="${APPDIR}/usr/fonts"
fi

# ------------------------------------------------------------------------------
# Smart Desktop & CLI Shortcut Integration Helper
# ------------------------------------------------------------------------------
install_shortcut() {
    local target_bin="${HOME}/.local/bin/nutrackn"
    local bin_dir="${HOME}/.local/bin"
    local apps_dir="${HOME}/.local/share/applications"
    local icons_dir="${HOME}/.local/share/icons/hicolor/512x512/apps"
    local icons_base="${HOME}/.local/share/icons/hicolor"
    local source_appimage="${APPIMAGE:-$SELF}"

    mkdir -p "${bin_dir}"
    mkdir -p "${apps_dir}"
    mkdir -p "${icons_dir}"
    mkdir -p "${HOME}/bin"

    echo "==> Integrating NuTrackN with your system..."

    # Install executable binary / copy AppImage
    if [ -n "${APPIMAGE}" ] && [ -f "${APPIMAGE}" ]; then
        cp -a "${APPIMAGE}" "${target_bin}"
        chmod +x "${target_bin}"
        echo "    ✓ Installed 'nutrackn' command to: ${target_bin}"
    else
        ln -sf "${SELF}" "${target_bin}"
        echo "    ✓ Created launcher: ${target_bin} -> ${SELF}"
    fi

    # Also symlink into ~/bin/nutrackn
    ln -sf "${target_bin}" "${HOME}/bin/nutrackn" 2>/dev/null || true

    # Install multi-resolution icons
    if [ -f "${APPDIR}/nutrackn.png" ]; then
        cp -a "${APPDIR}/nutrackn.png" "${icons_dir}/nutrackn.png"
    fi
    for s in 16 32 48 64 128 256; do
        if [ -d "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps" ]; then
            mkdir -p "${icons_base}/${s}x${s}/apps"
            cp -a "${APPDIR}/usr/share/icons/hicolor/${s}x${s}/apps/"* "${icons_base}/${s}x${s}/apps/" 2>/dev/null || true
        fi
    done
    echo "    ✓ Installed application icons"

    # Define desktop entry content with absolute icon path
    local desktop_entry="[Desktop Entry]
Version=1.0
Type=Application
Name=NuTrackN
GenericName=Nuclear Spectroscopy Analysis
Comment=Interactive Nuclear Spectroscopy & Gamma-Ray Analysis
Exec=${target_bin} %F
Icon=${icons_dir}/nutrackn.png
Terminal=false
Categories=Science;Physics;DataVisualization;Qt;
MimeType=application/x-root;
StartupNotify=true
StartupWMClass=nutrackn"

    # Install desktop shortcut on Desktop
    local desktop_dir=""
    if command -v xdg-user-dir >/dev/null 2>&1; then
        desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null)"
    fi
    if [ -z "${desktop_dir}" ] || [ ! -d "${desktop_dir}" ]; then
        desktop_dir="${HOME}/Desktop"
    fi
    mkdir -p "${desktop_dir}"

    if [ -d "${desktop_dir}" ]; then
        echo "${desktop_entry}" > "${desktop_dir}/NuTrackN.desktop"
        chmod +x "${desktop_dir}/NuTrackN.desktop"
        gio set "${desktop_dir}/NuTrackN.desktop" metadata::trusted true 2>/dev/null || true
        gio set "${desktop_dir}/NuTrackN.desktop" metadata::trusted yes 2>/dev/null || true
        echo "    ✓ Created shortcut on your Desktop: ${desktop_dir}/NuTrackN.desktop"
    fi

    # Install desktop entry in Applications menu
    echo "${desktop_entry}" > "${apps_dir}/nutrackn.desktop"
    chmod +x "${apps_dir}/nutrackn.desktop"
    echo "    ✓ Created Desktop menu shortcut: ${apps_dir}/nutrackn.desktop"

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${apps_dir}" 2>/dev/null || true
    fi

    # Ensure ~/.local/bin and ~/bin are configured across shell profiles
    for profile in "${HOME}/.bashrc" "${HOME}/.profile" "${HOME}/.bash_profile" "${HOME}/.zshrc"; do
        if [ -f "${profile}" ]; then
            if ! grep -q 'export PATH="$HOME/.local/bin:$PATH"' "${profile}" && ! grep -q 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' "${profile}"; then
                echo 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' >> "${profile}"
            fi
        fi
    done
    if [ ! -f "${HOME}/.bashrc" ]; then
        echo 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' >> "${HOME}/.bashrc"
    fi

    echo ""
    echo "======================================================================"
    echo "    🎉 1-Click Installation Complete! All is ready.                   "
    echo "======================================================================"
    echo "  🖥️  Desktop Shortcut:                                              "
    echo "      Double-click 'NuTrackN' on your Desktop!                       "
    echo "      (Also in Applications Menu -> Science -> NuTrackN)             "
    echo ""
    echo "  💻 Terminal Command:                                               "
    echo "      Installed to: ~/.local/bin/nutrackn                            "
    echo ""
    echo "  👉 To use 'nutrackn' in this terminal right now, run:              "
    echo "         source ~/.profile                                           "
    echo "      (or: source ~/.bashrc)                                         "
    echo ""
    echo "      In any new terminal window, simply type:                       "
    echo "         nutrackn                                                    "
    echo "======================================================================"
    echo ""
}

uninstall_shortcut() {
    echo "==> Removing NuTrackN shortcuts from system..."
    local desktop_dir=""
    if command -v xdg-user-dir >/dev/null 2>&1; then
        desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null)"
    fi
    [ -z "${desktop_dir}" ] && desktop_dir="${HOME}/Desktop"

    rm -f "${HOME}/.local/bin/nutrackn"
    rm -f "${HOME}/bin/nutrackn"
    rm -f "${HOME}/.local/share/applications/nutrackn.desktop"
    rm -f "${desktop_dir}/NuTrackN.desktop"
    rm -f "${desktop_dir}/nutrackn.desktop"
    rm -f "${HOME}/.local/share/icons/hicolor/512x512/apps/nutrackn.png"
    for s in 16 32 48 64 128 256; do
        rm -f "${HOME}/.local/share/icons/hicolor/${s}x${s}/apps/nutrackn.png"
    done
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    fi
    echo "    ✓ NuTrackN terminal command and desktop shortcuts removed successfully."
}

# Handle command-line options
case "$1" in
    --install|-i|--integrate)
        install_shortcut
        exit 0
        ;;
    --uninstall|-u|--remove)
        uninstall_shortcut
        exit 0
        ;;
    --help|-h)
        echo "NuTrackN - Interactive Nuclear Spectroscopy"
        echo ""
        echo "Usage: nutrackn [options] [spectrum_files...]"
        echo ""
        echo "Integration Options:"
        echo "  --install, -i    Install 'nutrackn' terminal command (~/.local/bin) & desktop shortcut"
        echo "  --uninstall, -u  Remove 'nutrackn' terminal command & desktop shortcut"
        echo "  --help, -h       Display this help message"
        echo ""
        exit 0
        ;;
esac

# If running directly from terminal and not yet installed in ~/.local/bin/nutrackn, display helpful tip
if [ -t 0 ] && [ ! -f "${HOME}/.local/bin/nutrackn" ]; then
    echo "----------------------------------------------------------------------"
    echo " 💡 Tip: Add the 'nutrackn' terminal command (just like xtrackn) with:"
    echo "         $0 --install"
    echo "----------------------------------------------------------------------"
fi

# Detect if host glibc satisfies requirement (GLIBC_2.38)
HOST_LIBC="$(ldconfig -p 2>/dev/null | awk '/libc\.so\.6/ { print $NF; exit }')"
[ -z "$HOST_LIBC" ] && [ -f /lib/x86_64-linux-gnu/libc.so.6 ] && HOST_LIBC="/lib/x86_64-linux-gnu/libc.so.6"
[ -z "$HOST_LIBC" ] && [ -f /lib64/libc.so.6 ] && HOST_LIBC="/lib64/libc.so.6"

USE_COMPAT_LOADER=false
if [ -f "${APPDIR}/usr/lib/compat/ld-linux-x86-64.so.2" ]; then
    if [ -n "$HOST_LIBC" ] && [ -f "$HOST_LIBC" ]; then
        if ! grep -a -q "GLIBC_2.38" "$HOST_LIBC" 2>/dev/null; then
            USE_COMPAT_LOADER=true
        fi
    else
        USE_COMPAT_LOADER=true
    fi
fi

if [ "$USE_COMPAT_LOADER" = true ]; then
    exec "${APPDIR}/usr/lib/compat/ld-linux-x86-64.so.2" \
         --library-path "${APPDIR}/usr/lib:${APPDIR}/usr/lib/compat:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/lib64:/usr/lib64" \
         "${APPDIR}/usr/bin/nutrackn" "$@"
else
    exec "${APPDIR}/usr/bin/nutrackn" "$@"
fi
EOF
chmod +x "${APPDIR}/AppRun"

# ------------------------------------------------------------------------------
# 5. Generate 1-Click Universal Standalone AppImage (Zero-Dependency, FUSE-Free)
# ------------------------------------------------------------------------------
echo "==> [5/6] Generating 1-Click Universal Standalone AppImage..."

GIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo "release")"
BUILD_ID="nutrackn-${GIT_HASH}-$(date +%Y%m%d)"

RUNNER_TMP="${BUILD_DIR}/runner_header.sh"
cat << 'RUNNER_EOF' > "${RUNNER_TMP}"
#!/usr/bin/env bash
# ==============================================================================
# NuTrackN - 1-Click Universal Standalone Linux Executable
# Zero External Dependencies • No FUSE Required • No Root Privileges Required
# Works out of the box on Ubuntu 20.04+, 22.04+, 24.04+, Debian, Fedora, Arch, etc.
# ==============================================================================
set -eo pipefail

SELF="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
APP_NAME="NuTrackN"
APP_EXE="nutrackn"
BUILD_ID="@@BUILD_ID@@"

# Locate archive payload separator
PAYLOAD_LINE=$(grep -a -m 1 -n '^__NUTRACKN_PAYLOAD_BELOW__$' "$SELF" 2>/dev/null | cut -d: -f1)
if [ -z "$PAYLOAD_LINE" ]; then
    echo "[-] Error: Embedded application payload could not be located in $SELF" >&2
    exit 1
fi

extract_payload() {
    local target_dir="$1"
    mkdir -p "$target_dir"
    tail -n +"$((PAYLOAD_LINE + 1))" "$SELF" | tar -xz -C "$target_dir"
}

install_system() {
    local install_dir="${HOME}/.local/share/nutrackn"
    local bin_dir="${HOME}/.local/bin"
    local apps_dir="${HOME}/.local/share/applications"
    local icons_base="${HOME}/.local/share/icons/hicolor"

    echo "======================================================================"
    echo "    🚀  Installing NuTrackN to User Environment (~/.local)...         "
    echo "======================================================================"
    
    mkdir -p "${install_dir}" "${bin_dir}" "${apps_dir}" "${icons_base}/512x512/apps"
    mkdir -p "${HOME}/bin"

    echo "==> [1/4] Extracting application payload to ${install_dir}..."
    rm -rf "${install_dir}"/*
    extract_payload "${install_dir}"
    chmod +x "${install_dir}/AppRun" "${install_dir}/usr/bin/${APP_EXE}"
    echo "    ✓ Application runtime and libraries installed"

    echo "==> [2/4] Setting up 'nutrackn' terminal command in ${bin_dir}..."
    cat << RUNNER > "${bin_dir}/${APP_EXE}"
#!/usr/bin/env bash
exec "${install_dir}/AppRun" "\$@"
RUNNER
    chmod +x "${bin_dir}/${APP_EXE}"
    echo "    ✓ Created launcher command: ${bin_dir}/${APP_EXE}"

    # Also symlink into ~/bin/nutrackn
    ln -sf "${bin_dir}/${APP_EXE}" "${HOME}/bin/${APP_EXE}" 2>/dev/null || true

    # If /usr/local/bin is writable or passwordless sudo is available, install system-wide command
    if [ -w "/usr/local/bin" ]; then
        ln -sf "${install_dir}/AppRun" "/usr/local/bin/${APP_EXE}" 2>/dev/null && echo "    ✓ Added system command: /usr/local/bin/${APP_EXE}" || true
    elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        sudo -n ln -sf "${install_dir}/AppRun" "/usr/local/bin/${APP_EXE}" 2>/dev/null && echo "    ✓ Added system command: /usr/local/bin/${APP_EXE}" || true
    fi

    echo "==> [3/4] Installing application icons..."
    if [ -f "${install_dir}/nutrackn.png" ]; then
        cp -a "${install_dir}/nutrackn.png" "${icons_base}/512x512/apps/${APP_EXE}.png"
    fi
    for s in 16 32 48 64 128 256; do
        if [ -d "${install_dir}/usr/share/icons/hicolor/${s}x${s}/apps" ]; then
            mkdir -p "${icons_base}/${s}x${s}/apps"
            cp -a "${install_dir}/usr/share/icons/hicolor/${s}x${s}/apps/"* "${icons_base}/${s}x${s}/apps/" 2>/dev/null || true
        fi
    done
    echo "    ✓ Application icons installed"

    echo "==> [4/4] Creating Desktop and system menu shortcuts..."
    local desktop_entry="[Desktop Entry]
Version=1.0
Type=Application
Name=NuTrackN
GenericName=Nuclear Spectroscopy Analysis
Comment=Interactive Nuclear Spectroscopy & Gamma-Ray Analysis
Exec=${bin_dir}/${APP_EXE} %F
Icon=${icons_base}/512x512/apps/${APP_EXE}.png
Terminal=false
Categories=Science;Physics;DataVisualization;Qt;
MimeType=application/x-root;
StartupNotify=true
StartupWMClass=nutrackn"

    # Install on user's Desktop
    local desktop_dir=""
    if command -v xdg-user-dir >/dev/null 2>&1; then
        desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null)"
    fi
    if [ -z "${desktop_dir}" ] || [ ! -d "${desktop_dir}" ]; then
        desktop_dir="${HOME}/Desktop"
    fi
    mkdir -p "${desktop_dir}"

    if [ -d "${desktop_dir}" ]; then
        echo "${desktop_entry}" > "${desktop_dir}/NuTrackN.desktop"
        chmod +x "${desktop_dir}/NuTrackN.desktop"
        gio set "${desktop_dir}/NuTrackN.desktop" metadata::trusted true 2>/dev/null || true
        gio set "${desktop_dir}/NuTrackN.desktop" metadata::trusted yes 2>/dev/null || true
        echo "    ✓ Created shortcut on your Desktop: ${desktop_dir}/NuTrackN.desktop"
    fi

    # Install in Applications Menu
    echo "${desktop_entry}" > "${apps_dir}/${APP_EXE}.desktop"
    chmod +x "${apps_dir}/${APP_EXE}.desktop"
    echo "    ✓ Created application menu entry: ${apps_dir}/${APP_EXE}.desktop"

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${apps_dir}" 2>/dev/null || true
    fi

    # Ensure ~/.local/bin and ~/bin are configured in shell profiles
    for profile in "${HOME}/.bashrc" "${HOME}/.profile" "${HOME}/.bash_profile" "${HOME}/.zshrc"; do
        if [ -f "${profile}" ]; then
            if ! grep -q 'export PATH="$HOME/.local/bin:$PATH"' "${profile}" && ! grep -q 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' "${profile}"; then
                echo 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' >> "${profile}"
            fi
        fi
    done
    if [ ! -f "${HOME}/.bashrc" ]; then
        echo 'export PATH="$HOME/.local/bin:$HOME/bin:$PATH"' >> "${HOME}/.bashrc"
    fi

    echo ""
    echo "======================================================================"
    echo "    🎉 1-Click Installation Complete! All is ready.                   "
    echo "======================================================================"
    echo "  🖥️  Desktop Shortcut:                                              "
    echo "      Double-click 'NuTrackN' on your Desktop!                       "
    echo "      (Also in Applications Menu -> Science -> NuTrackN)             "
    echo ""
    echo "  💻 Terminal Command:                                               "
    echo "      Installed to: ~/.local/bin/nutrackn                            "
    echo ""
    echo "  👉 To use 'nutrackn' in this terminal right now, run:              "
    echo "         source ~/.profile                                           "
    echo "      (or: source ~/.bashrc)                                         "
    echo ""
    echo "      In any new terminal window, simply type:                       "
    echo "         nutrackn                                                    "
    echo "======================================================================"
    echo ""
}

uninstall_system() {
    local install_dir="${HOME}/.local/share/nutrackn"
    local bin_dir="${HOME}/.local/bin"
    local apps_dir="${HOME}/.local/share/applications"
    local icons_base="${HOME}/.local/share/icons/hicolor"
    local desktop_dir=""
    if command -v xdg-user-dir >/dev/null 2>&1; then
        desktop_dir="$(xdg-user-dir DESKTOP 2>/dev/null)"
    fi
    [ -z "${desktop_dir}" ] && desktop_dir="${HOME}/Desktop"

    echo "==> Uninstalling NuTrackN from system..."
    rm -rf "${install_dir}"
    rm -f "${bin_dir}/${APP_EXE}"
    rm -f "${HOME}/bin/${APP_EXE}"
    if [ -w "/usr/local/bin/${APP_EXE}" ]; then
        rm -f "/usr/local/bin/${APP_EXE}"
    fi
    rm -f "${apps_dir}/${APP_EXE}.desktop"
    rm -f "${desktop_dir}/NuTrackN.desktop"
    rm -f "${desktop_dir}/${APP_EXE}.desktop"
    rm -f "${icons_base}/512x512/apps/${APP_EXE}.png"
    for s in 16 32 48 64 128 256; do
        rm -f "${icons_base}/${s}x${s}/apps/${APP_EXE}.png"
    done
    rm -rf "${HOME}/.cache/nutrackn"
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${apps_dir}" 2>/dev/null || true
    fi
    echo "    ✓ NuTrackN removed successfully from system."
}

show_help() {
    echo "NuTrackN - Interactive Nuclear Spectroscopy"
    echo ""
    echo "Usage: ./NuTrackN-x86_64.AppImage [options] [spectrum_files...]"
    echo ""
    echo "Options:"
    echo "  --install, -i        Install 'nutrackn' terminal command & desktop shortcut"
    echo "  --uninstall, -u      Remove 'nutrackn' terminal command & desktop shortcut"
    echo "  --extract-to DIR     Extract application files to specified directory"
    echo "  --version, -v        Display version and build information"
    echo "  --help, -h           Display this help message"
    echo ""
    echo "Example:"
    echo "  chmod +x NuTrackN-x86_64.AppImage"
    echo "  ./NuTrackN-x86_64.AppImage --install"
    echo "  nutrackn spectrum.spe"
    echo ""
}

case "$1" in
    --install|-i|--integrate)
        install_system
        exit 0
        ;;
    --uninstall|-u|--remove)
        uninstall_system
        exit 0
        ;;
    --help|-h)
        show_help
        exit 0
        ;;
    --version|-v)
        echo "NuTrackN standalone bundle (Build: ${BUILD_ID})"
        exit 0
        ;;
    --extract-to)
        if [ -z "$2" ]; then
            echo "[-] Error: --extract-to requires a destination directory." >&2
            exit 1
        fi
        echo "==> Extracting NuTrackN to: $2..."
        extract_payload "$2"
        echo "    ✓ Extraction completed."
        exit 0
        ;;
esac

# Direct execution (without --install)
CACHE_DIR="${HOME}/.cache/nutrackn/${BUILD_ID}"
if [ ! -f "${CACHE_DIR}/AppRun" ]; then
    echo "==> [NuTrackN] Preparing standalone environment (one-time setup, ~4s)..."
    rm -rf "${HOME}/.cache/nutrackn"/*
    extract_payload "${CACHE_DIR}"
    chmod +x "${CACHE_DIR}/AppRun" "${CACHE_DIR}/usr/bin/${APP_EXE}"
fi

if [ -t 0 ] && [ ! -f "${HOME}/.local/bin/${APP_EXE}" ]; then
    echo "----------------------------------------------------------------------"
    echo " 💡 Tip: Add the 'nutrackn' terminal command (just like xtrackn) with:"
    echo "         $SELF --install"
    echo "----------------------------------------------------------------------"
fi

exec "${CACHE_DIR}/AppRun" "$@"

__NUTRACKN_PAYLOAD_BELOW__
RUNNER_EOF

sed -i "s/@@BUILD_ID@@/${BUILD_ID}/" "${RUNNER_TMP}"

# Assemble standalone AppImage
echo "    Packaging standalone self-executing bundle..."
cp "${RUNNER_TMP}" "${APPIMAGE_OUTPUT}"
tar -czf - -C "${APPDIR}" . >> "${APPIMAGE_OUTPUT}"
chmod +x "${APPIMAGE_OUTPUT}"
rm -f "${RUNNER_TMP}"

# Also generate a standalone portable tarball
echo "==> Packaging standalone portable tarball..."
TARBALL_OUTPUT="${DIST_DIR}/${APP_NAME}-linux-x86_64-portable.tar.gz"
(
    cd "${BUILD_DIR}"
    rm -rf "${APP_NAME}"
    cp -a "AppDir" "${APP_NAME}"
    tar -czf "${TARBALL_OUTPUT}" "${APP_NAME}"
    rm -rf "${APP_NAME}"
)

# ------------------------------------------------------------------------------
# 6. Summary
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "    🎉 Linux Packages Built Successfully!                            "
echo "======================================================================"
echo "    1. Standalone Universal AppImage (100% FUSE-Free, Zero-Dependency):"
echo "       ${APPIMAGE_OUTPUT} ($(du -h "${APPIMAGE_OUTPUT}" | awk '{print $1}'))"
echo ""
echo "    2. Portable Tarball:"
echo "       ${TARBALL_OUTPUT} ($(du -h "${TARBALL_OUTPUT}" | awk '{print $1}'))"
echo ""
echo "----------------------------------------------------------------------"
echo " 💡 Standard 1-Click Workflow on ANY Linux computer:"
echo "    chmod +x $(basename "${APPIMAGE_OUTPUT}")"
echo "    ./$(basename "${APPIMAGE_OUTPUT}") --install"
echo "    nutrackn"
echo "======================================================================"


