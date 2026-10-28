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
ROOT_CFLAGS="$(root-config --cflags 2>/dev/null || echo '-std=c++20')"
"${QMAKE_BIN}" nutrackn.pro -spec macx-clang "CONFIG+=sdk_no_version_check" "CONFIG+=c++17" "CONFIG+=c++20" "QMAKE_CXXFLAGS_CXX17=-std=c++20" "QMAKE_CXXFLAGS_CXX1Z=-std=c++20" "QMAKE_CXXFLAGS+=-std=c++20 ${ROOT_CFLAGS}"

# Enforce -std=c++20 across the generated Makefile, replacing any old C++17/C++14/C++11/C++98 defaults injected by Qt mkspecs
if [ -f Makefile ]; then
    sed -i.bak -E 's/-std=(c\+\+11|gnu\+\+11|c\+\+14|gnu\+\+14|c\+\+1y|gnu\+\+1y|c\+\+17|gnu\+\+17|c\+\+1z|gnu\+\+1z|gnu\+\+98|c\+\+98)/-std=c++20/g' Makefile
fi

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

# Resolve CERN ROOT subdirectories (supports both Homebrew lib/root layout and standard binary tarballs)
ROOT_LIBDIR="$(root-config --libdir 2>/dev/null || echo "${ROOTSYS}/lib")"
[ ! -d "${ROOT_LIBDIR}" ] && [ -d "${ROOTSYS}/lib/root" ] && ROOT_LIBDIR="${ROOTSYS}/lib/root"
ROOT_ETCDIR="$(root-config --etcdir 2>/dev/null || echo "${ROOTSYS}/etc")"
[ ! -d "${ROOT_ETCDIR}" ] && [ -d "${ROOTSYS}/etc/root" ] && ROOT_ETCDIR="${ROOTSYS}/etc/root"
ROOT_INCDIR="$(root-config --incdir 2>/dev/null || echo "${ROOTSYS}/include")"
[ ! -d "${ROOT_INCDIR}" ] && [ -d "${ROOTSYS}/include/root" ] && ROOT_INCDIR="${ROOTSYS}/include/root"
ROOT_FONTSDIR="${ROOTSYS}/fonts"
[ ! -d "${ROOT_FONTSDIR}" ] && [ -d "${ROOTSYS}/share/root/fonts" ] && ROOT_FONTSDIR="${ROOTSYS}/share/root/fonts"

# Copy ROOT runtime data (etc/, fonts/, include/) and prune unused subsystems
if [ -d "${ROOT_ETCDIR}" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/etc"
    cp -a "${ROOT_ETCDIR}/"* "${APP_BUNDLE}/Contents/Resources/root/etc/" 2>/dev/null || true
    rm -rf "${APP_BUNDLE}/Contents/Resources/root/etc/notebook" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/proof" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/html" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/http" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/daemons" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/vmc" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/pdg_table.txt" \
           "${APP_BUNDLE}/Contents/Resources/root/etc/RadioNuclides.txt" 2>/dev/null || true
fi
if [ -d "${ROOT_FONTSDIR}" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/fonts"
    cp -a "${ROOT_FONTSDIR}/"* "${APP_BUNDLE}/Contents/Resources/root/fonts/" 2>/dev/null || true
    rm -f "${APP_BUNDLE}/Contents/Resources/root/fonts/DroidSansFallback.ttf" \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/FreeSerif"*.otf \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/FreeMono"*.otf \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/BlackChancery.ttf" \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/comic"*.ttf \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/impact.ttf" \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/webdings.ttf" \
          "${APP_BUNDLE}/Contents/Resources/root/fonts/wingding.ttf" 2>/dev/null || true
fi
if [ -d "${ROOT_INCDIR}" ]; then
    mkdir -p "${APP_BUNDLE}/Contents/Resources/root/include"
    cp -a "${ROOT_INCDIR}/"* "${APP_BUNDLE}/Contents/Resources/root/include/" 2>/dev/null || true
    rm -rf "${APP_BUNDLE}/Contents/Resources/root/include/TMVA" \
           "${APP_BUNDLE}/Contents/Resources/root/include/RooFit"* \
           "${APP_BUNDLE}/Contents/Resources/root/include/RooStats" \
           "${APP_BUNDLE}/Contents/Resources/root/include/Minuit2" \
           "${APP_BUNDLE}/Contents/Resources/root/include/bvh" \
           "${APP_BUNDLE}/Contents/Resources/root/include/ROOT/RDataFrame"* \
           "${APP_BUNDLE}/Contents/Resources/root/include/ROOT/RNTuple"* \
           "${APP_BUNDLE}/Contents/Resources/root/include/ROOT/REve"* \
           "${APP_BUNDLE}/Contents/Resources/root/include/ROOT.modulemap" 2>/dev/null || true
fi

# ------------------------------------------------------------------------------
# 4. Deploy Qt Frameworks & ROOT Dynamic Libraries
# ------------------------------------------------------------------------------
echo "==> [4/6] Deploying Qt5 frameworks & slimmed CERN ROOT dynamic libraries..."

# Run macdeployqt targeting the actual Mach-O binary (nutrackn_bin)
if [ -x "${MACDEPLOYQT_BIN}" ]; then
    echo "    Running macdeployqt..."
    "${MACDEPLOYQT_BIN}" "${APP_BUNDLE}" -executable="${APP_BUNDLE}/Contents/MacOS/nutrackn_bin" -always-overwrite 2>&1 || true
fi

# Copy essential CERN ROOT dynamic libraries & PCMs
if [ -d "${ROOT_LIBDIR}" ]; then
    echo "    Bundling essential ROOT dylibs and Cling dictionaries from ${ROOT_LIBDIR}..."
    ROOT_ESSENTIAL_MODULES=(
        Core Cling RIO Thread Imt Rint
        MathCore Matrix Minuit Fumili Physics
        Hist HistPainter Spectrum SpectrumPainter
        Graf Graf3d Gpad Postscript ASImage ASImageGui
        GQuartz GCocoa GX11 GX11TTF Gui Ged FitPanel Tree Net
    )
    for mod in "${ROOT_ESSENTIAL_MODULES[@]}"; do
        cp -a "${ROOT_LIBDIR}/lib${mod}.dylib"* "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
        cp -a "${ROOT_LIBDIR}/lib${mod}.so"* "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
        cp -a "${ROOT_LIBDIR}/lib${mod}_rdict.pcm" "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
        cp -a "${ROOT_LIBDIR}/${mod}.pcm" "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
    done

    ROOT_CORE_PCMS=(
        _Builtin_intrinsics _Builtin_stddef
        Cling_Runtime Cling_Runtime_Extra
        ROOT_Config ROOT_Foundation_C ROOT_Foundation_Stage1_NoRTTI ROOT_Rtypes
        std libc
    )
    for pcm in "${ROOT_CORE_PCMS[@]}"; do
        cp -a "${ROOT_LIBDIR}/${pcm}.pcm" "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
    done
fi

# Ensure libssh2 is bundled in Frameworks if not already copied by macdeployqt
LIBSSH2_PREFIX="$(brew --prefix libssh2 2>/dev/null || echo '/opt/homebrew/opt/libssh2')"
if [ -d "${LIBSSH2_PREFIX}/lib" ]; then
    cp -a "${LIBSSH2_PREFIX}/lib/libssh2"*.dylib "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
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

# Strip local debug symbols to reduce bundle size
if command -v strip >/dev/null 2>&1; then
    echo "    Stripping local debug symbols from Mach-O binaries..."
    strip -x "${APP_BUNDLE}/Contents/MacOS/nutrackn_bin" 2>/dev/null || true
    find "${APP_BUNDLE}/Contents/Frameworks" -type f \( -name "*.dylib" -o -name "*.so" \) -exec strip -x {} + 2>/dev/null || true
fi

# Re-sign the application bundle (mandatory on Apple Silicon arm64 after install_name_tool / strip)
if command -v codesign >/dev/null 2>&1; then
    echo "    Applying ad-hoc code signature to NuTrackN.app..."
    codesign --force --deep --sign - "${APP_BUNDLE}" 2>/dev/null || true
fi

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
