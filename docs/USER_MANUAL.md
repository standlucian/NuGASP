# NuTrackN (NuGASP) Official User Manual

**Version 2.0** — *Nuclear Gamma-Ray & Particle Spectroscopy Analysis Environment*  
**Architecture**: Modern C++17 / Qt5 Core + CERN ROOT Scientific Graphics Engine  
**Quick Help Shortcut**: Press **`H`** or **`?`** (or **`F1`**) on the spectrum canvas at any time.

---

## Table of Contents
- [1. Introduction, Heritage & Design Philosophy](#1-introduction-heritage--design-philosophy)
    - [1.1 The Heritage of Xtrackn & GASPware](#11-the-heritage-of-xtrackn--gaspware)
    - [1.2 Historical Attributions & Acknowledgments](#12-historical-attributions--acknowledgments)
    - [1.3 AI Assistance & Development Disclosure](#13-ai-assistance--development-disclosure)
    - [1.4 Development Roadmap & Wishlist](#14-development-roadmap--wishlist)
    - [1.5 The NuGASP Ecosystem Scope](#15-the-nugasp-ecosystem-scope)
    - [1.6 Software Stack, Dependencies & Modernization Roadmap](#16-software-stack-dependencies--modernization-roadmap)
    - [1.7 Software License & Academic Citation](#17-software-license--academic-citation)
- [2. Installation, Deployment & Quick Start](#2-installation-deployment--quick-start)
    - [2.1 Obtaining Pre-Built Packages (GitHub Releases)](#21-obtaining-pre-built-packages-github-releases)
    - [2.2 Linux (1-Click AppImage)](#22-linux-1-click-appimage)
    - [2.3 macOS (Drag-and-Drop DMG for Apple Silicon & Intel)](#23-macos-drag-and-drop-dmg-for-apple-silicon--intel)
    - [2.4 Building from Source](#24-building-from-source)
    - [2.5 First Spectrum Walkthrough in 60 Seconds](#25-first-spectrum-walkthrough-in-60-seconds)
- [3. Data Input, Output (I/O) & File Management](#3-data-input-output-io--file-management)
    - [3.1 Supported Nuclear Spectroscopy Formats](#31-supported-nuclear-spectroscopy-formats)
    - [3.2 Loading Spectra & Multi-Buffer Overlays (Buffers 1–9)](#32-loading-spectra--multi-buffer-overlays-buffers-19)
    - [3.3 Modern File Dialog (NuFileDialog) & Live Previews](#33-modern-file-dialog-nufiledialog--live-previews)
    - [3.4 Remote Data Acquisition & SSH Session Manager](#34-remote-data-acquisition--ssh-session-manager)
    - [3.5 Exporting Processed Data](#35-exporting-processed-data)
- [4. Canvas Navigation, Scaling & Real-Time HUD](#4-canvas-navigation-scaling--real-time-hud)
    - [4.1 Display Scales & Vertical Adjustments](#41-display-scales--vertical-adjustments)
    - [4.2 Horizontal Panning & Zooming](#42-horizontal-panning--zooming)
    - [4.3 Mouse Navigation & Interactive Marker Dragging](#43-mouse-navigation--interactive-marker-dragging)
    - [4.4 Real-Time HUD Peak Inspector](#44-real-time-hud-peak-inspector)
- [5. Peak Integration & Region of Interest (ROI) Analysis](#5-peak-integration--region-of-interest-roi-analysis)
    - [5.1 Setting Boundary Markers](#51-setting-boundary-markers)
    - [5.2 Mathematical Formulation of Gross & Net Peak Area](#52-mathematical-formulation-of-gross--net-peak-area)
    - [5.3 Counting Statistics & Error Propagation](#53-counting-statistics--error-propagation)
    - [5.4 Marker Management & Clearing](#54-marker-management--clearing)
    - [5.5 Output Report Logging](#55-output-report-logging)
- [6. Standard Energy & Efficiency Calibration](#6-standard-energy--efficiency-calibration)
    - [6.1 Calibration Physics Formulation](#61-calibration-physics-formulation)
    - [6.2 Fast 2-Point Calibration Routine (K)](#62-fast-2-point-calibration-routine-k)
    - [6.3 Full Polynomial Calibration Dialog (DK)](#63-full-polynomial-calibration-dialog-dk)
    - [6.4 Automated Standard Radioisotope Calibration (AK)](#64-automated-standard-radioisotope-calibration-ak)
    - [6.5 Detector Full-Energy Peak Efficiency Calibration (DE)](#65-detector-full-energy-peak-efficiency-calibration-de)
- [7. Advanced Peak Fitting & Multiplet Deconvolution](#7-advanced-peak-fitting--multiplet-deconvolution)
    - [7.1 Physical Peak Models (Gaussian & Low-Energy Tail)](#71-physical-peak-models-gaussian--low-energy-tail)
    - [7.2 Step Backgrounds: ERFC Compton Plateau & Median Sidebands](#72-step-backgrounds-erfc-compton-plateau--median-sidebands)
    - [7.3 Poisson Maximum Likelihood Estimation vs. Least-Squares](#73-poisson-maximum-likelihood-estimation-vs-least-squares)
    - [7.4 Dynamic Peak Extent, Valley Search & Physical Capping](#74-dynamic-peak-extent-valley-search--physical-capping)
    - [7.5 Multiplet Deconvolution & Parameter Linking/Locking](#75-multiplet-deconvolution--parameter-linkinglocking)
    - [7.6 Goodness of Fit & Residuals Inspection](#76-goodness-of-fit--residuals-inspection)
- [8. In-Beam Run-by-Run Drift Alignment (Flagship Workflow)](#8-in-beam-run-by-run-drift-alignment-flagship-workflow)
    - [8.1 Physics of Thermal & Rate Gain Drifts in Long Experiments](#81-physics-of-thermal--rate-gain-drifts-in-long-experiments)
    - [8.2 Sequence Preparation & Anchor Peak Configuration](#82-sequence-preparation--anchor-peak-configuration)
    - [8.3 Automated Drift Tracking Engine](#83-automated-drift-tracking-engine)
    - [8.4 Status Interpretation: OK, INTERVENE, and FAILED](#84-status-interpretation-ok-intervene-and-failed)
    - [8.5 The Interactive Intervention Dialog & Live Floating Canvas HUD](#85-the-interactive-intervention-dialog--live-floating-canvas-hud)
    - [8.6 High-Statistics Chunking & Hardware Jump-Aware Smoothing](#86-high-statistics-chunking--hardware-jump-aware-smoothing)
    - [8.7 Export Formats: Legacy GASP .cal vs. Extended Multi-Order .ucal](#87-export-formats-legacy-gasp-cal-vs-extended-multi-order-ucal)
- [9. 2D Coincidence Matrix Analysis](#9-2d-coincidence-matrix-analysis)
    - [9.1 Foundations of Gamma-Gamma Coincidence Matrices](#91-foundations-of-gamma-gamma-coincidence-matrices)
    - [9.2 Inspecting Matrices (Q & MatrixDialog)](#92-inspecting-matrices-q--matrixdialog)
    - [9.3 Coincidence Gating (CW) & Background Subtraction (DW)](#93-coincidence-gating-cw--background-subtraction-dw)
- [10. Scripting, Macros & Automation](#10-scripting-macros--automation)
    - [10.1 Graphical Interface Focus & Terminal Status Notice](#101-graphical-interface-focus--terminal-status-notice)
    - [10.2 Supported Automation Tokens & Command Strings](#102-supported-automation-tokens--command-strings)
    - [10.3 Numeric Macros (0 through 9) & Repetition Cycles](#103-numeric-macros-0-through-9--repetition-cycles)
    - [10.4 The Macro Manager Dialog (DM)](#104-the-macro-manager-dialog-dm)
    - [10.5 Saving, Loading & Distributing Macro Files (.mac)](#105-saving-loading--distributing-macro-files-mac)
- [11. Aesthetic Customization & Theme Management](#11-aesthetic-customization--theme-management)
    - [11.1 Built-in Themes (Modern Dark, Modern Light, Classic Xtrackn)](#111-built-in-themes)
    - [11.2 Customizing Palette Colors, Gridlines & Typography](#112-customizing-palette-colors-gridlines--typography)
    - [11.3 Theme Import and Export (.nugasp-theme)](#113-theme-import-and-export-nugasp-theme)
- [12. End-to-End Analysis Cookbook Workflows](#12-end-to-end-analysis-cookbook-workflows)
    - [12.1 Deconvoluting an Overlapping Multiplet (`R` → `G` → `CV`)](#121-deconvoluting-an-overlapping-multiplet-r--g--cv)
    - [12.2 Batch Integrating Across Detectors with a Macro (`*1` + `CJ`)](#122-batch-integrating-across-detectors-with-a-macro-1--cj)
    - [12.3 Correcting Gain Drift Across 100+ In-Beam Runs (`AR`)](#123-correcting-gain-drift-across-100-in-beam-runs-ar)
- [13. Troubleshooting & Frequently Asked Questions (FAQ)](#13-troubleshooting--frequently-asked-questions-faq)
    - [13.1 Zoom & Marker Placement Issues (`E`, `CI`, `CJ`)](#131-zoom--marker-placement-issues-e-ci-cj)
    - [13.2 Resolving `INTERVENE` and `FAILED` States in Run-by-Run Alignment](#132-resolving-intervene-and-failed-states-in-run-by-run-alignment)
    - [13.3 Low-Statistics Peak Fitting & Baseline Instabilities](#133-low-statistics-peak-fitting--baseline-instabilities)
- [14. Appendices & Technical Specifications](#14-appendices--technical-specifications)
    - [14.1 Master Keyboard Command & Shortcut Reference](#141-master-keyboard-command--shortcut-reference)
    - [14.2 File Format Specifications](#142-file-format-specifications)
    - [14.3 Coordinate System & Binning Conventions (0-Indexed Channels)](#143-coordinate-system--binning-conventions-0-indexed-channels)
    - [14.4 System Requirements & Supported Platforms](#144-system-requirements--supported-platforms)

---

## 1. Introduction, Heritage & Design Philosophy

**NuTrackN** (the flagship analysis environment of the **NuGASP** suite) is an interactive, high-performance nuclear spectroscopy application designed for experimental nuclear physicists, gamma-ray spectroscopists, and radiation detection laboratories.

### 1.1 The Heritage of Xtrackn & GASPware
For decades, the classic Fortran/C software **Xtrackn** and the larger **GASPware suite** (developed across European and international heavy-ion and gamma-ray facilities like GASP, AGATA, and EUROBALL) served as the gold standard for high-throughput gamma-gamma matrix gating and spectrum analysis. Experimentalists developed deep muscle memory for its single-character and two-character command strings.

**NuTrackN preserves 100% of authentic Xtrack keyboard commands**, while re-engineering the underlying platform into modern C++17 with:
- **CERN ROOT Graphics Engine**: Sub-millisecond rendering of high-statistics histograms and logarithmic axes.
- **Modern Qt5 User Interface**: High-DPI screen support, responsive dialogs, live parameter tables, and curated dark/light themes.
- **2D Matrix Coincidence Viewer**: Fast slicing and projection of multi-gigabyte γ-γ coincidence matrices (`.cmat`).
- **Autonomous Standalone Deployment**: 1-click execution without requiring users to configure compiler paths or install CERN ROOT manually.
- **Run-by-Run Drift Alignment**: Automated in-beam gain tracking across hundreds of acquisition runs.

> [!NOTE]
> **Operational Model & Terminal Notice:**  
> NuTrackN keyboard commands are executed directly on the interactive spectrum canvas (hotkeys). Due to the current focus on user-friendly dialog windows, interactive parameter tables, and graphical workflows, **program operation through the terminal (either the system terminal / stdin or the in-built bottom console) is not currently functional for command input.** The in-built bottom console serves exclusively as an output log for calculation reports, marker moment summaries, and fit parameters. Terminal-driven command execution and headless batch processing with no GUI will be the topic of a later upgrade.

### 1.2 Historical Attributions & Acknowledgments
The algorithms, design philosophies, and core workflows of NuTrackN stand upon decades of foundational work by the creators and maintainers of **GASPware** and **Xtrackn**, originally created for the GASP γ-ray spectrometer at the INFN Laboratori Nazionali di Legnaro (LNL) and Padova, Italy.

We gratefully acknowledge and credit:
- **Dino Bazzacco** (*INFN Sezione di Padova*): Main designer and author of the core GASP data analysis programs, including **TRACKN**, **CMAT**, and **GSORT**.
- **Călin A. Ur** (*INFN Padova / IFIN-HH Bucharest*): Co-author and core collaborator on the **GSORT** event-sorting engine.
- **Nicolae Mărginean** (*INFN LNL / IFIN-HH Bucharest*): Longtime maintainer and developer of GASPware, author of individual peak-width fitting routines in XTRACKN, asynchronous tape/data I/O, canvas enhancements, and cross-platform Unix/Linux/macOS ports.

Special thanks are also due to the authors of key open-source utilities and algorithms integrated into the original GASP ecosystem:
- **Fred Hucht** (*Universität Duisburg*): Creator of the **Ygl** library, which provided SGI GL graphics emulation under the X Window System.
- **Takuji Nishimura & Makoto Matsumoto**: Creators of the **Mersenne Twister** pseudorandom number generator used in event simulation and background generation.
- The authors at **M.S.I. Stockholm** and the **Niels Bohr Institute (NBI)** for the **laslib** PostScript plotting packages.

### 1.3 AI Assistance & Development Disclosure
NuTrackN was developed through human-directed, AI-assisted software engineering using Google DeepMind's Antigravity (Gemini) coding agent. AI assistance was utilized for porting legacy Fortran and C routines into modern C++17/Qt5, designing UI dialogs and graphics canvas integrations, structuring automated packaging scripts, and drafting documentation.

All mathematical formulations, nuclear physics algorithms (e.g., SNIP continuum extraction, Poisson maximum likelihood estimation, covariance error propagation), and calibration procedures have been verified, scientifically reviewed, and validated against experimental gamma-ray data.

However, it is important to note that, while credit for code synthesis and acceleration belongs to the AI agents, full responsibility for any errors and oversights rests solely with the human maintainers. Should you encounter unexpected behavior or software bugs, please open an issue on GitHub. Our apologies in advance for any rough edges that may crop up!

### 1.4 Development Roadmap & Wishlist
NuTrackN is an actively maintained open-source environment. While the 2.0 release delivers a high-performance modern Qt5/ROOT GUI foundation, historical command compatibility, and automated run-by-run drift alignment, the following capabilities represent key priorities for upcoming development cycles:

#### 1. Rigorous End-to-End Uncertainty Propagation
- **Coincidence Gating & Matrix Slicing**: Full bin-by-bin variance and covariance tracking through 2D background-subtracted gating ($S_{\text{net}} = S_{\text{gate}} - \frac{A_{\text{gate}}}{A_{\text{bg}}} S_{\text{bg}}$), ensuring downstream peak fits on gated projections correctly propagate Poisson counting statistics and background subtraction uncertainties.
- **Peak Fitting & Multiplet Deconvolution**: Extracting and propagating full Minuit parameter covariance matrices ($\text{cov}(A, \mu, \sigma)$) and asymmetric confidence intervals (MINOS) to avoid underestimating peak area errors under heavy baseline coupling.
- **Energy & Efficiency Calibrations**: Seamless propagation of polynomial parameter covariances ($\text{cov}(a_i, a_j)$), literature decay standard uncertainties, and source half-life corrections directly into calibrated energy readouts ($E \pm \sigma_E$) and detector efficiency curves.

#### 2. Nuclear Reaction & Lifetime Analysis Tools
- **Rudimentary DSAM Fitting**: Doppler Shift Attenuation Method line-shape analysis for picosecond nuclear state lifetime determinations.
- **Rudimentary RDDS Fitting**: Recoil Distance Doppler Shift (plunger) lifetime measurements and shifted/unshifted intensity decay curve fitting.
- **Angular Distributions, Correlations & DCO Fitting**: Directional Correlations from Oriented states (DCO) and angular distribution / correlation fitting for multipole mixing ratio and spin/parity assignments.

#### 3. Advanced Calibrations & Peak Response Modeling
- **Multi-Region Calibration Alignment**: Interactive creation, boundary alignment, and production of piecewise multi-region energy calibrations across wide dynamic ranges.
- **Improved Efficiency Calibration Creation**: Flexible empirical efficiency curve modeling, absorption correction handling, and interactive residual inspection.
- **Advanced Non-Gaussian Peak Response Functions**: Expanding the fitting engine beyond symmetric Gaussians to incorporate realistic detector physics (low-energy exponential tailing, high-energy neutron damage tailing, skewed Gaussians, and Voigt profiles).

#### 4. Spectra Manipulation & GASPware Tools Integration
- **Native SADD Functionality**: Interactive and batch spectrum addition, gain-matched summing across detectors and experimental runs, and exporting combined spectra.
- **Full CMAT Functionality Integration**: Native embedding of legacy CMAT utilities for coincidence matrix creation, projection, 2D inspection, and matrix symmetrization.
- **Direct CERN ROOT Format Data Operations**: Reading, manipulating, and exporting native ROOT format data (`TH1`, `TH2`, and `TTree`) directly without intermediate ASCII conversion.

#### 5. Interface Modernization & Batch Automation
- **Remaking the Main Interface**: Redesigning the main application canvas, controls, and docking system for multi-monitor setups and flexible spectroscopy workflows.
- **Terminal Operation & Headless Batch Processing**: Restoring full command execution via system terminal `stdin` and enabling headless batch processing flags to execute analysis pipelines with no GUI.
- **Python & Scientific Ecosystem Integration**: Lightweight bindings and serialization bridges to manipulate NuTrackN calibration trajectories and spectra within NumPy, SciPy, and Jupyter environments.

We warmly welcome ideas, feedback, and contributions from experimentalists and spectroscopy laboratories worldwide. If you have suggestions, feature requests, bug reports, or would simply like to send coffee our way, please feel free to open an issue on [GitHub](https://github.com/standlucian/NuGASP) or reach out directly by email at `lucian.stan@nipne.ro`.

### 1.5 The NuGASP Ecosystem Scope
The **NuGASP** suite is conceived as the modern C++17 / Qt5 successor to the offline analysis ecosystem of **GASPware**. Its overarching goal is to modernize and consolidate the classic tools developed for the GASP spectrometer—including spectrum visualization (`xtrackn`), 2D matrix projection and slicing (`cmat`), spectrum arithmetic and gain-matched addition (`sadd`), efficiency parameterization (`effi`), initial polynomial calibrations (`precal`), and run-by-run drift minimization (`trackfit`)—into a unified, high-performance computing environment.

The sole deliberate exception to this scope is **`gsort`** (the raw event-sorting and list-mode reconstruction engine). Due to its specialized hardware-level data unpacking, real-time trigger handling, and online acquisition requirements, `gsort` remains an independent standalone program outside NuTrackN.

### 1.6 Software Stack, Dependencies & Modernization Roadmap

NuTrackN is engineered as a high-performance scientific desktop application combining the computational rigor of low-level C++ with modern graphical interfaces and nuclear physics data visualization.

#### 1. Current Dependency Stack

| Component | Technology / Library | Version / Target | Purpose & Scope |
| :--- | :--- | :--- | :--- |
| **Language Standard** | **ISO C++17** | `-std=c++17` | High-performance compiled core, filesystem access (`<filesystem>`), structured bindings, and lambda-based numerical optimization. Compatible with GCC 11–14, Clang 14+, and Apple Clang. |
| **GUI & Application Core** | **Qt5** | `5.15 LTS` | Main window architecture, high-DPI scaling, dialog managers, multi-threading (`QThread`), networking (`QtNetwork`), and cross-platform UI event loops. |
| **Scientific Graphics & Engine** | **CERN ROOT** | `6.24` – `6.32+` | High-throughput 1D histogramming (`TH1F`/`TH1D`), 2D coincidence matrix structures (`TH2D`), Minuit numerical minimization, peak deconvolution, and PostScript vector export (`TPostScript`). |
| **Remote Connectivity** | **libssh2** | `1.9+` | Native SSH-2 / SFTP client protocol implementation for remote DAQ browsing, cache synchronization, and real-time beam run streaming. |
| **Build Tooling** | **CMake** / **QMake** | `CMake 3.16+` | Dual build support enabling rapid Qt Creator IDE development (`nutrackn.pro`) and automated command-line builds (`CMakeLists.txt`). |

#### 2. Modernization & Version Upgrade Plans

To ensure long-term sustainability across evolving laboratory operating systems and high-performance computing clusters, the NuGASP development roadmap outlines the following upgrade milestones:

##### Migration to Qt 6 (Qt 6.5+ LTS)
- **Wayland & Native Desktop Support**: Transitioning the UI layer to Qt 6 provides seamless native support for modern Linux Wayland compositors (reducing rendering latency and eliminating fractional scaling blur) as well as updated macOS Apple Silicon windowing.
- **Rendering Hardware Interface (RHI)**: Leveraging Qt's modern RHI abstractions (Vulkan, Metal, Direct3D 12, OpenGL) to accelerate custom canvas overlays and interactive multi-detector grid layouts.
- **Legacy API Deprecation**: Completing the transition away from older Qt5 classes, standardizing entirely on `QRegularExpression`, `QStringView`, and type-safe compile-time signal-slot connections.

##### Language Standard Evolution ($\text{C++20} \rightarrow \text{C++23}$)
- **Zero-Copy Views with `std::span` and Ranges**: Replacing pointer-and-size buffer passes with `std::span` and standard range adaptors across spectrum parsing routines and 2D coincidence matrix slicing. This eliminates transient heap allocations during heavy gating operations.
- **Concepts & Type Constraints**: Enhancing template metaprogramming across multi-format spectrum loaders (`LongInt32`, `ShortInt16`, `Float32`, `Double64`) with clear, compiler-enforced C++20 concepts (`std::integral`, `std::floating_point`), delivering cleaner compilation diagnostics.
- **Type-Safe String Formatting (`std::format`)**: Phasing out legacy `printf`-style formatting and complex stream concatenation in favor of `std::format` for calibration reports, log entries, and export headers.

##### CERN ROOT Modernization & Graphics Pipeline
- **Continuous Compatibility with ROOT 6.32+ & ROOT 7**: Ensuring NuTrackN seamlessly compiles against the latest CERN ROOT releases and tracking upcoming ROOT 7 graphics and object management paradigms.
- **Next-Generation Coincidence Rendering**: Evaluating modern hardware-accelerated shaders and WebGUI graphics integration to allow remote researchers to inspect heavy gigabyte-scale coincidence matrices directly through web-native browser canvas views.
- **High-Throughput Batch Processing with `ROOT::RDataFrame`**: Leveraging multi-threaded columnar data processing (`RDataFrame`) for background-subtracted gating and event sorting pipelines.

##### Unified Build System & Automated CI/CD
- **CMake Single Source of Truth**: Consolidating on Modern CMake (3.25+) with `CMakePresets.json` as the single canonical build configuration, phasing out legacy `.pro` files while maintaining 1-click Qt Creator integration.
- **Automated Multi-Platform Packaging**: Expanding automated GitHub Actions workflows to continuously package standalone Linux AppImages, Flatpaks, native Debian/RPM packages, and universal macOS DMG binaries on every tagged release.

### 1.7 Software License & Academic Citation

#### Software License (GNU GPLv3)
NuTrackN is free open-source software licensed under the **GNU General Public License v3.0 (GPLv3)**.

Under this "copyleft" license, you are free to inspect, run, modify, and redistribute the software for academic, research, or commercial purposes. However, to protect the scientific community and ensure that improvements remain permanently open, any distributed software that incorporates or derives from NuTrackN must also be licensed under the GNU GPLv3 with its full source code made publicly available.

#### Academic Citation
If you use NuTrackN for spectral analysis, coincidence matrix slicing, or gain alignment in scientific publications, master's theses, or doctoral dissertations, please cite the software as:

> **Text Citation:**  
> L. Stan, *NuTrackN: Modern Nuclear Gamma-Ray and Particle Spectroscopy Analysis Environment*, Version 2.0, Horia Hulubei National Institute for R&D in Physics and Nuclear Engineering (IFIN-HH / NIPNE), GitHub repository: https://github.com/standlucian/NuGASP (2026).

```bibtex
@misc{stan_nutrackn_2026,
  author       = {Stan, Lucian},
  title        = {{NuTrackN: Modern Nuclear Gamma-Ray and Particle Spectroscopy Analysis Environment}},
  year         = {2026},
  version      = {2.0},
  institution  = {Horia Hulubei National Institute for R&D in Physics and Nuclear Engineering (IFIN-HH)},
  publisher    = {GitHub},
  howpublished = {\url{https://github.com/standlucian/NuGASP}}
}
```

For scientific attribution of the foundational algorithms and original spectrometer architecture, please also cite:
> D. Bazzacco, *The GASP Spectrometer at Legnaro*, International Conference on Nuclear Structure at High Angular Momentum, Ottawa, AECL-10613, pp. 376–387 (1992).

---

## 2. Installation, Deployment & Quick Start

NuTrackN is distributed both as pre-compiled, self-contained standalone binary packages (with all Qt5 and CERN ROOT runtime libraries bundled inside) and as open-source code ready to compile from source.

### 2.1 Obtaining Pre-Built Packages (GitHub Releases)
Official pre-compiled executables and disk images for Linux and macOS are hosted on the project's GitHub Releases page:

> 🌐 **Official Release Portal:**  
> **[https://github.com/standlucian/NuGASP/releases](https://github.com/standlucian/NuGASP/releases)**

To download the latest image for your platform:
1. Navigate to the **[Latest Release](https://github.com/standlucian/NuGASP/releases/latest)**.
2. Scroll down to the **Assets** accordion at the bottom of the release notes.
3. Select the package corresponding to your operating system:
   - **Linux (x86_64, all distributions with glibc 2.28+)**: Download **`NuTrackN-x86_64.AppImage`**.
   - **macOS (Apple Silicon M1 / M2 / M3 / M4)**: Download **`NuTrackN-macOS-arm64.dmg`**.
   - **macOS (Intel Core x86_64)**: Download **`NuTrackN-macOS-x86_64.dmg`**.

> [!TIP]
> **Zero External Dependencies Required!**  
> Both the Linux AppImage and macOS DMG bundles embed all required CERN ROOT graphics engines, Qt runtime libraries, and mathematical solvers. You do not need to install ROOT, configure environment variables (`$ROOTSYS`), or set library paths on your machine to run the pre-built packages.

#### Development Snapshots (GitHub Actions Artifacts)
In addition to tagged releases, continuous integration snapshots compiled from the latest `main` branch commits can be downloaded directly from GitHub Actions:
1. Visit **[GitHub Actions Workflows](https://github.com/standlucian/NuGASP/actions)**.
2. Select the latest completed run of **Build & Package Linux** or **Build & Package macOS**.
3. Scroll down to the **Artifacts** section at the bottom of the summary page to download the pre-built bundle (`NuTrackN-Linux-x86_64` or `NuTrackN-macOS-arm64`).

### 2.2 Linux (1-Click AppImage)
NuTrackN is distributed as a completely self-contained Linux AppImage with embedded Qt5 and CERN ROOT runtimes:

```bash
# 1. Make executable
chmod +x NuTrackN-x86_64.AppImage

# 2. Add 'nutrackn' terminal command and Desktop Launcher (1-click)
./NuTrackN-x86_64.AppImage --install

# 3. Launch from anywhere in terminal
nutrackn
```
*(To remove: run `nutrackn --uninstall`).*

### 2.3 macOS (Drag-and-Drop DMG for Apple Silicon & Intel)
1. Download `NuTrackN-macOS-arm64.dmg` (Apple Silicon M1/M2/M3/M4) or `NuTrackN-macOS-x86_64.dmg` (Intel Core) from the [Releases page](https://github.com/standlucian/NuGASP/releases).
2. Open the disk image and drag **`NuTrackN.app`** into your `/Applications` folder.
3. To enable the command line shortcut in Terminal:
   ```bash
   /Applications/NuTrackN.app/Contents/MacOS/nutrackn --install-cli
   ```
4. *(Note for macOS Gatekeeper: If prompted that the developer cannot be verified upon first launch, right-click (or Control-click) `NuTrackN.app` in `/Applications` and select **Open**, then confirm **Open**).*

### 2.4 Building from Source
If building in a custom research laboratory environment:
```bash
# Prerequisites: CMake 3.16+, Qt5 (Widgets, Network), CERN ROOT 6.24+
git clone https://github.com/standlucian/NuGASP.git
cd NuGASP/NuTrackN
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./nutrackn
```

### 2.5 First Spectrum Walkthrough in 60 Seconds
1. Launch `nutrackn`.
2. Press **`N`** (or click the spectrum open button) and select your spectrum file (e.g. `152Eu_sample.spe`).
3. Press **`L`** to toggle between Linear and Logarithmic scale.
4. Hover the cursor over a photopeak; the **Live Peak Inspector Box** in the upper right dynamically displays centroid, channel, counts, and local FWHM.
5. Move cursor to the left of the peak and press **`[`**; move to the right and press **`]`**. Press **`E`** to zoom between the markers.
6. Press **`CJ`** to calculate net peak area with automatic linear background subtraction.

---

## 3. Data Input, Output (I/O) & File Management

### 3.1 Supported Nuclear Spectroscopy Formats
NuTrackN automatically detects and parses all standard nuclear physics data formats:
- **Ortec Maestro (`.spe`, `.chn`)**: Standard multichannel analyzer ASCII and binary records with header timing and live/real time tags.
- **Canberra Formats & Generic ASCII (`.txt`, `.dat`, `.csv`)**: Single-column counts or two-column $(Channel, Counts)$ with space, tab, or comma separation.
- **Radware (`.spe`)**: Integer and floating-point Radware spectrum buffers.
- **CERN ROOT Histograms (`.root`)**: 1D histograms (`TH1D`, `TH1F`, `TH1I`) and 2D matrices (`TH2D`).
- **Compressed Coincidence Matrices (`.cmat`, `.mat`)**: GASPware symmetrized and raw 2D gamma coincidence matrices.

### 3.2 Loading Spectra & Multi-Buffer Overlays (Buffers 1–9)
NuTrackN supports multi-pad canvas grids alongside multi-spectrum overlays on any individual pad:
- **Multi-Pad Grid Layout**:
  - Use **`Ctrl + →`** to add a new display column and **`Ctrl + ←`** to remove a column.
  - Use **`Ctrl + ↑`** and **`Ctrl + ↓`** to add or remove display rows.
  - Click inside any sub-window to make it the active pad (highlighted by a colored frame border).
- **Multi-Buffer Overlays & Stepping**:
  - Up to 9 spectra can be overlaid within the same pad (`HijC` overlay stack), each rendered with its distinct theme palette color.
  - Use **`N` / `N+` / `*1`** and **`N-` / `*2`** to step forward or backward through spectra/detectors, and **`*3` (`R+` / `NR`)** or **`*4` (`R-` / `PR`)** to step forward or backward across experimental runs.
  - *(Note: On the canvas keyboard, single digits **`0`–`9`** trigger stored numeric macros; see [Section 10.2](#102-numeric-macros-0-through-9--repetition-cycles)).*
- **Cross-Pad & Overlay Synchronization**:
  - Use **`SX`** / **`SY`** to propagate the active pad's horizontal (X) or vertical (Y) scale and linear/logarithmic mode across all visible sub-canvases and their overlaid spectra.

### 3.3 Modern File Dialog (NuFileDialog) & Live Previews
NuTrackN features an embedded high-speed file browser (`NuFileDialog`):
- **Live Spectrum Mini-Preview**: Click any spectrum file to inspect its raw channel count and shape in real time before opening.
- **Fast Navigation Bookmarks**: Quick access to experiment data directories, USB mount points, and home directories.
- **Format Filter Filtering**: Dynamic switching between `.spe`, `.chn`, `.root`, and all ASCII variations.

### 3.4 Remote Data Acquisition & SSH Session Manager
During in-beam accelerator experiments, acquisition data often resides on remote DAQ machines or storage nodes. NuTrackN includes a dedicated SSH Client and Session Manager:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Remote SSH Session Manager                           │
├────────────────────────────────────────────────────────────────────────┤
│ Host: daq-cluster.lab.internal     Port: 22     User: expuser          │
│ Auth: [•] Private Key (~/.ssh/id_ed25519)   [ ] Password               │
│ Remote Directory: /data/exp2026/run_calib/                              │
│                                                                        │
│ [Test Connection]                Status: Connected (Authenticated)     │
└────────────────────────────────────────────────────────────────────────┘
```

1. **Configuring Profiles**:
   - Open the Remote Connect Dialog from the toolbar or calibration sequence selector.
   - Enter Host IP/hostname, Port (default: 22), Username, and select SSH Key Authentication (`.pem`, `.id_rsa`, `id_ed25519`) or Password.
   - Save the profile for instant 1-click reconnection in future sessions.
2. **Browsing & Auto-Syncing**:
   - The remote file dialog allows navigating remote directories seamlessly.
   - When running Run-by-Run Drift Alignment, NuTrackN downloads remote run files asynchronously in background threads with live progress bars, caching them locally in your working session.

### 3.5 Exporting Processed Data
- **`OS`**: Export the active spectrum data (channel, calibrated energy, counts) to a space-delimited ASCII table.
- **`O=`**: Generate a publication-quality vector PostScript (`.ps`) or PDF graphic of the current canvas view.
- **`DF`**: Designate an output report logfile for automated recording of peak integration areas and fit parameters.
- **`ZF`**: Close the active area calculation logfile.

---

## 4. Canvas Navigation, Scaling & Real-Time HUD

### 4.1 Display Scales & Vertical Adjustments
- **`L`**: Toggle between **Linear** and **Logarithmic** Y-axis display (automatically clamping log minimum to `0.5` counts and computing peak headroom across both the primary histogram and any overlaid spectra).
- **`U` / `D`**: Vertically expand or compress the active histogram counts.
- **`FO`**: Set the vertical axis maximum (Y-Max) directly to the count level under the cursor.
- **`FU`**: Set the vertical axis minimum (Y-Min) directly to the count level under the cursor.
- **`MZ`**: Draw a reference baseline line at zero counts.

### 4.2 Horizontal Panning & Zooming
- **`E` (Expand)**: Zooms the horizontal axis to fit the region between the two most recently placed **`Spacebar`** markers.
- **`X`**: Centers the view around the current cursor location with 2× magnification.
- **`<` / `>`**: Pan the visible spectrum window 75% to the left or right.
- **`FF` (or `FS`)**: Full display reset (both horizontal and vertical axes auto-scale to full dataset, equivalent to `FX` + `FY`).
- **`FX` / `FY`**: Reset only the horizontal (energy/channel) or vertical (counts, with configurable 10% linear / 30% log headroom above the highest visible peak across all overlays) scale.
- **`=`**: Redraw and refresh canvas.

### 4.3 Mouse Navigation & Interactive Marker Dragging
- **Rubber-band Box Zoom**: Left-click and drag a rectangular region over any spectrum feature to zoom directly into that bounding box.
- **Interactive Marker Dragging**: Left-click and drag any active marker along the spectrum curve to fine-tune integration or fitting limits interactively.
- **Clickable Axis Labels**: Left-click or right-click (with optional `Ctrl` modifier for fine/coarse stepping) the `X Min`, `X Max`, `Y Min`, or `Y Max` readout boxes on the **top status header bar** to adjust limits in discrete steps.

### 4.4 Real-Time HUD Peak Inspector
Located in the upper right quadrant of the spectrum canvas, the **HUD Inspector** computes real-time statistics for whatever feature the cursor is hovering over:
- **Cursor Energy & Channel**: Exact interpolated calibration coordinate.
- **Counts / Bin**: Discrete histogram content $C(ch)$.
- **Local Centroid**: Moment-calculated peak center within cursor proximity:
  $$\langle ch \rangle = \frac{\sum_{i \in \text{win}} i \cdot C_i}{\sum_{i \in \text{win}} C_i}$$
- **Local FWHM**: Instantaneous full-width at half-maximum derived from local variance.

---

## 5. Peak Integration & Region of Interest (ROI) Analysis

### 5.1 Setting Boundary Markers
- **`Spacebar`**: Place generic analysis marker at mouse pointer.
- **`B`**: Place Background marker (`Blue`).
- **`I`**: Place Integration boundary marker (`Yellow`).
- **`[` and `]`**: Convenient left and right boundary placement.

### 5.2 Mathematical Formulation of Gross & Net Peak Area
Given a photopeak bounded by a pair of integration markers ($ch_{min}$ and $ch_{max}$), let $y_i$ denote the bin content at channel center $x_i$:

1. **Gross Integrated Area ($G$)**:
   Calculated by the **`CI`** command without baseline subtraction:
   $$G = \sum_{i = ch_{min}}^{ch_{max}} y_i$$

2. **Least-Squares Linear Background Estimation (`CB` / `CJ`)**:
   Background markers (**`B`**) are placed in consecutive pairs $[b_{1,\text{left}}, b_{1,\text{right}}], [b_{2,\text{left}}, b_{2,\text{right}}], \dots$ defining arbitrary background intervals on either side of the peak(s). Across all $N_{bg}$ channels enclosed by these background marker pairs, NuTrackN performs **ordinary least-squares (OLS) linear regression** to determine the best-fit baseline:
   $$B(x) = m \cdot x + c$$
   where the slope $m$ and intercept $c$ are evaluated from the background sample means $(\bar{x}, \bar{y})$:
   $$m = \frac{\sum_{k=1}^{N_{bg}} (x_k - \bar{x})(y_k - \bar{y})}{\sum_{k=1}^{N_{bg}} (x_k - \bar{x})^2}, \quad c = \bar{y} - m \cdot \bar{x}$$

3. **Net Peak Area ($N$)**:
   Calculated by the **`CJ`** command (or automatically at the cursor via **`AJ`**) by subtracting the interpolated linear background $b_i = m \cdot x_i + c$ bin-by-bin across each pair of integration markers:
   $$N \equiv M_0 = \sum_{i = ch_{min}}^{ch_{max}} (y_i - b_i) = \sum_{i = ch_{min}}^{ch_{max}} y_{\text{net}, i}$$

### 5.3 Counting Statistics, Statistical Moments & Error Propagation
NuTrackN implements the authentic Xtrackn / GASPware statistical moment and variance propagation algorithm relative to the interval midpoint $x_{\text{mid}} = \frac{1}{2}(ch_{min} + ch_{max})$. Let $w_i = x_i - x_{\text{mid}}$ be the centered channel offset:

1. **Net Moments ($M_0, M_1, M_2$) and Variance Moments ($D_0, D_1, D_2, D_4$)**:
   For each bin $i$ in the integration window, the gross Poisson variance is $\sigma^2(y_i) = \max(y_i, 1)$ and the background variance contribution is $\sigma^2(b_i) = \max(b_i, 0)$, yielding a per-bin net variance $v_i = \max(y_i, 1) + \max(b_i, 0)$. The moments are accumulated as:
   $$M_k = \sum_{i = ch_{min}}^{ch_{max}} y_{\text{net}, i} \, w_i^k \quad (k = 0, 1, 2), \qquad D_n = \sum_{i = ch_{min}}^{ch_{max}} v_i \, w_i^n \quad (n = 0, 1, 2, 4)$$

2. **Net Area Uncertainty ($\sigma_N$)**:
   $$\sigma_N = \sqrt{D_0} = \sqrt{\sum_{i = ch_{min}}^{ch_{max}} \left(\max(y_i, 1) + \max(b_i, 0)\right)}$$

3. **First-Moment Centroid ($Ch_0$) & Uncertainty ($\sigma_{Ch_0}$)**:
   With $\delta x = M_1 / M_0$, the background-subtracted centroid and its propagated variance are:
   $$Ch_0 = x_{\text{mid}} + \delta x, \qquad \sigma^2(Ch_0) = \frac{D_2 - 2\,\delta x\,D_1 + (\delta x)^2 D_0}{M_0^2}$$

4. **Second-Moment Peak Width (FWHM) & 4th-Moment Error Propagation**:
   From the central variance $s^2 = (M_2 / M_0) - (\delta x)^2$, the statistical FWHM and its uncertainty are evaluated as:
   $$\text{FWHM} = 2\sqrt{2\ln 2}\, s \approx 2.354820 \, \sqrt{s^2}$$
   $$\sigma(\text{FWHM}) = \frac{\text{FWHM}}{2 s^2} \sqrt{4(\delta x)^2 \sigma^2(Ch_0) + \frac{D_4}{M_0^2} - \frac{2 M_2 D_2}{M_0^3} + \frac{M_2^2 D_0}{M_0^4}}$$

5. **Spectroscopic Bracketed Uncertainty Formatting**:
   Results printed to the output console and terminal log use standard nuclear spectroscopy parenthetical notation for the least significant digits—for example, an energy of `1332.49(5)` keV denotes $1332.49 \pm 0.05\text{ keV}$, and an area of `14520(135)` denotes $14520 \pm 135\text{ counts}$.

### 5.4 Marker Management & Clearing
- **`ZA`**: Erase all active markers from the canvas.
- **`ZB`**: Erase only background (`B`) markers.
- **`ZI`**: Erase only integration (`I`) markers.
- **`ZR`**: Erase only fit range (`R`) markers.
- **`ZG`**: Erase only Gaussian peak (`G`) markers.
- **`ZW`**: Erase only coincidence gate (`W`) markers.
- **`ZJ` / `ZV`**: Erase auto-integration (`AJ`) or multi-fit (`CV`) markers.
- **`MP` / `ZP`**: Show or clear automated peak-search (`CP`) labels.
- **`MI` / `MJ`**: Re-display and highlight integration markers.

### 5.5 Output Report Logging
Use **`DF`** to open a destination logfile. Every subsequent integration (**`CI`** / **`CJ`**) or peak fit (**`CG`** / **`CV`**) automatically appends a formatted entry containing timestamp, centroid, FWHM, gross counts, net counts, and statistical error. Use **`ZF`** to close the file.

---

## 6. Standard Energy & Efficiency Calibration

### 6.1 Calibration Physics Formulation
Spectroscopy channel numbers ($ch$) are mapped to physical gamma-ray energies ($E$, in keV) using a polynomial calibration model:
$$E(ch) = a_0 + a_1 \cdot ch + a_2 \cdot ch^2$$
where:
- $a_0$: Electronic zero offset / ADC intercept (in keV).
- $a_1$: Primary gain / dispersion slope (in keV/channel).
- $a_2$: Second-order non-linearity parameter (typically $|a_2| < 10^{-6}\text{ keV/ch}^2$).

The detector energy resolution (Full Width at Half Maximum) is parameterized as a function of energy:
$$\text{FWHM}^2(E) = w_0 + w_1 \cdot E + w_2 \cdot E^2$$
accounting for electronic noise ($w_0$), charge collection statistics / Fano factor ($w_1$), and incomplete charge collection ($w_2$).

### 6.2 Fast 2-Point Calibration Routine (`K`)
For quick experimental checks during setup:
1. Place markers on two known photopeaks (e.g., the 511.0 keV positron annihilation line and 1274.5 keV $^{22}\text{Na}$ line).
2. Press **`K`** and enter the two literature energies when prompted.
3. NuTrackN calculates $a_0$ and $a_1$ instantaneously and updates the canvas axis readout to keV.

### 6.3 Full Polynomial Calibration Dialog (`DK`)
Press **`DK`** to open the interactive Polynomial Calibration Manager:
1. Add calibration points $(ch_i, E_{lit, i})$ manually or by clicking peaks.
2. Select polynomial degree (Linear: 1, Quadratic: 2).
3. Click **Fit** to perform weighted least-squares regression:
   $$\chi^2 = \sum_{i=1}^M \left( \frac{E_{lit, i} - (a_0 + a_1 ch_i + a_2 ch_i^2)}{\sigma(E_i)} \right)^2$$
4. Review the parameter correlation matrix and fit residuals $\Delta E_i = E_{lit, i} - E_{calc, i}$.
5. Click **Save Calibration** to export to an ASCII `.mcal` file.

### 6.4 Automated Standard Radioisotope Calibration (`AK`)
Press **`AK`** to open the Automated Calibration Engine:
1. Select the calibration standard source from the catalog:
   - **$^{60}\text{Co}$**: 1173.228 keV, 1332.492 keV
   - **$^{137}\text{Cs}$**: 661.657 keV
   - **$^{133}\text{Ba}$**: 81.0, 276.4, 302.9, 356.0, 383.8 keV
   - **$^{152}\text{Eu}$**: Multi-line standard (121.8 through 1408.0 keV)
2. NuTrackN executes an automated peak hunt, identifies characteristic energy spacing ratios, associates peaks with literature lines, and fits the calibration polynomial automatically.

### 6.5 Detector Full-Energy Peak Efficiency Calibration (`DE`)
The absolute full-energy peak efficiency at energy $E_γ$ is determined from standard calibrated radioactive sources:
$$\epsilon(E_γ) = \frac{N_{net}(E_γ)}{A_{act} \cdot I_γ \cdot t_{live}}$$
where:
- $N_{net}$: Measured net peak counts.
- $A_{act}$: Source activity at the measurement epoch, corrected for radioactive decay:
  $$A_{act}(t) = A_0 \cdot \exp\left( -\frac{\ln 2 \cdot \Delta t}{T_{1/2}} \right)$$
- $I_γ$: Absolute emission probability per decay.
- $t_{live}$: Measurement live time (seconds).

Press **`DE`** to open the **Efficiency Calibration Dialog**:
- Select the fitting model:
  1. **Log-Log Polynomial**:
     $$\ln \epsilon = \sum_{k=0}^n c_k (\ln E)^k$$
  2. **Dual-Regime Power Law**: Combining low-energy absorption and high-energy pair production regimes.
- Click **Apply** to bind the efficiency calibration to the active session. Unknown sample activities are then calculated automatically from measured peak areas.

---

## 7. Advanced Peak Fitting & Multiplet Deconvolution

### 7.1 Physical Peak Models (Gaussian & Low-Energy Tail)
In semiconductor gamma detectors (HPGe, Si(Li)), incomplete charge collection due to crystal impurities or radiation damage causes low-energy peak tailing. NuTrackN models photopeaks with two primary response functions:

1. **Symmetric Gaussian Model**:
   $$g(x) = A \cdot \exp\left( -\frac{1}{2}\left(\frac{x - x_0}{\sigma}\right)^2 \right)$$
   where $A$ is peak amplitude, $x_0$ is centroid channel, and $\sigma$ is Gaussian dispersion ($\text{FWHM} = 2.35482 \cdot \sigma$).

2. **Skewed Gaussian with Exponential Low-Energy Tail**:
   $$f(x) = \begin{cases} 
      A \cdot \exp\left( -\frac{(x - x_0)^2}{2\sigma^2} \right) & \text{for } x \ge x_0 - t \cdot \sigma \\
      A \cdot \exp\left( \frac{t^2}{2} + \frac{t(x - x_0)}{\sigma} \right) & \text{for } x < x_0 - t \cdot \sigma 
   \end{cases}$$
   where $t > 0$ specifies the join point in units of standard deviation $\sigma$.

### 7.2 Step Backgrounds: ERFC Compton Plateau & Median Sidebands
Beneath every gamma photopeak lies a step in the background caused by multiple Compton scattering of photons that escape the detector before depositing full energy. NuTrackN models this background using an analytic complementary error function ($\text{erfc}$):

$$B(x) = m \cdot x + c + S \cdot \frac{1}{2} \text{erfc}\left( \frac{x - x_0}{\sqrt{2}\sigma} \right)$$
where:
- $m$: Background slope across the ROI.
- $c$: Baseline offset.
- $S$: Compton step amplitude.

```
Counts
  ^                 Photopeak Apex
  │                      /\
  │                     /  \
  │                    /    \
  │  Compton Step     /      \
  │  ─────────┐      /        \
  │      S    │     /          \
  │           └────/            \────────────── Baseline (Right)
  │               /              \
  └───────────────────────────────────────────────────> Channel (x)
                   <── x0 ──>
```

> [!NOTE]
> To eliminate mathematical collinearity between the slope ($m$) and step parameter ($S$) that can produce unphysical "sawtooth" baseline artifacts, NuTrackN employs robust median sidebands and dynamically caps the physical step height to a fraction of the raw peak amplitude ($S \le 0.15 \cdot A$).

### 7.3 Poisson Maximum Likelihood Estimation vs. Least-Squares
Standard $\chi^2$ minimizations assume Gaussian-distributed count errors ($\sigma_i = \sqrt{C_i}$). For low-statistics peaks ($C_i < 20$ counts/bin), this approximation fails and produces a systematic negative bias in peak area.

NuTrackN implements **Poisson Maximum Likelihood Estimation** (ROOT option `"L"` with `"I"` bin integration). Given measured bin counts $y_i$ and model expectation $\mu_i(p)$:
$$\ln L(p) = \sum_{i=1}^M \left[ y_i \ln \mu_i(p) - \mu_i(p) - \ln(y_i!) \right]$$
Minimizing the objective function $-2 \ln L$ guarantees unbiased centroid and area convergence even on weak peaks with zero-count bins. If likelihood optimization fails due to extreme background conditions, the engine automatically falls back to Minuit-improved least-squares (`"Q M R S"`).

### 7.4 Dynamic Peak Extent, Valley Search & Physical Capping
When fitting peaks in congested regions:
- **Outward Valley Search**: Starting from the centroid, the fitting routine searches left and right for local minima (valleys).
- **Dynamic Extent Window**: The fitting boundaries automatically contract if an adjacent doublet peak is detected, preventing the baseline from absorbing neighboring photopeaks.

### 7.5 Multiplet Deconvolution & Parameter Linking/Locking
To deconvolute overlapping multiplets:
1. Place Fit Range markers (**`R`**) enclosing the entire cluster.
2. Place centroid markers (**`G`** or **`+`**) on each visible apex.
3. Press **`CV`** to execute the multi-component fit.
4. Open the **Fit Parameters Dialog (`DG`)** to configure constraints:
   - **Link Widths**: Enforce identical $\sigma$ across all peaks in the multiplet ($\sigma_1 = \sigma_2 = \dots = \sigma_k$).
   - **Lock Centroid / Width**: Fix specific parameters to known values (e.g., from literature line separation) while allowing peak areas to float freely.

### 7.6 Goodness of Fit & Residuals Inspection
Upon convergence, NuTrackN computes the reduced chi-square:
$$\chi^2 / \text{NDF} = \frac{1}{M - K} \sum_{i=1}^M \frac{(y_i - \mu_i)^2}{\mu_i}$$
where $M$ is the number of fitted channels and $K$ is the number of free parameters. The interactive parameter table highlights fits with $\chi^2/\text{NDF} \le 1.8$ in green, while elevated values indicate unresolved multiplets or non-standard baseline behavior.

---

## 8. In-Beam Run-by-Run Drift Alignment (Flagship Workflow)

### 8.1 Physics of Thermal & Rate Gain Drifts in Long Experiments
Modern nuclear physics experiments typically collect data over several days or weeks across dozens or hundreds of individual runs. Environmental temperature swings in experimental vaults and fluctuating beam currents cause subtle shifts in photomultiplier or preamplifier gains. If uncorrected, summing these runs results in severe Doppler/energy resolution degradation.

NuTrackN's **Run-by-Run Drift Alignment Manager (`AR`)** solves this by tracking known anchor photopeaks run-by-run and computing precise time-dependent calibration polynomials.

```
  Run 001: ──────[ Peak 1332 keV: ch 2664.2 ]──────> Calib a1 = 0.50000 keV/ch
  Run 045: ──────[ Peak 1332 keV: ch 2668.5 ]──────> Calib a1 = 0.49919 keV/ch (Thermal Drift)
  Run 120: ──────[ Peak 1332 keV: ch 2661.1 ]──────> Calib a1 = 0.50058 keV/ch (Rate Shift)
```

### 8.2 Sequence Preparation & Anchor Peak Configuration
1. Open the **Run-by-Run Manager** by pressing **`AR`** or clicking the **Run-by-Run** toolbar button.
2. **Load Run Sequence & Configure Detector Array**:
   - Select a sequence file or batch directory containing spectra for all experimental runs (e.g., `run0001.spe` through `run0250.spe`), either locally or over an active SSH session.
   - Specify the **Detector Range** (`detectorStart` to `detectorEnd`, e.g., Detectors `0` to `24`), **Spectrum Channel Length** (e.g., `4096` (4k), `8192` (8k), `16384` (16k), `65536` (64k)), **Binary Format** (`LongInt32`, `ShortInt16`, `Float32`, etc.), **Nominal Dispersion** (in keV/ch, e.g., `1.0` or `0.5`), and **Polynomial Order** (Linear $a_0, a_1$ or Quadratic $a_0, a_1, a_2$).
3. **Configure Anchor Peaks**:
   - Add 1 to 5 prominent, well-isolated reference gamma lines present throughout the beam time (e.g., 511.0 keV annihilation, 1460.8 keV $^{40}\text{K}$, or strong in-beam reaction transitions).
   - Enter physical energy ($E_{ref} \pm \sigma_E$ in keV), reference channel ($ch_{ref}$, or `0` for auto-find from nominal dispersion), minimum required peak counts, expected FWHM, and the **Search Window** ($\pm \Delta ch$, default: $\pm 5$ channels).

### 8.3 Automated Drift Tracking Engine
Click **Run Calibration**:
- The engine (`RunByRunEngine`) processes all detector trajectories across all runs asynchronously in background threads without freezing the user interface (and supports non-blocking cancellation at any time).
- For each `(Run, Detector)` pair, the engine searches within the $\pm \Delta ch$ window around the last valid centroid trajectory, initializes peak parameters, and executes a Poisson-likelihood Gaussian + Compton step fit.
- If a run fails quality checks (e.g., beam trip or low statistics), the engine can automatically inherit the calibration from the most recent valid run (`isFallbackFromPrevious`) and record the source run index (`fallbackSourceRun`).

### 8.4 Status Interpretation: OK, INTERVENE, and FAILED
Each `(Run, Detector)` cell in the sequence matrix is classified into one of three operational states, backed by specific diagnostic status codes (`RunCalibStatus`):

| Status Badge | Meaning | Diagnostic Reasons (`RunCalibStatus`) & Action Required |
| :--- | :--- | :--- |
| 🟢 **OK** | **Clean Convergence** | `Success`: Peak was found within the search window; fit converged within `maxAllowedChi2NDF` and expected physical FWHM. No manual action needed. |
| 🟠 **INTERVENE** | **Manual Review Advised** | `ExcessiveDrift`, `BadChi2`, `BadFWHM`, or `LowStatistics`: Peak converged, but exhibited drift approaching `maxAllowedDriftCh`, elevated $\chi^2/\text{NDF}$, broadened/narrowed width, or weak statistics. Click **Intervene** to inspect and approve. |
| 🔴 **FAILED** | **Convergence Failure** | `NoPeakInWindow`, `RefAnchorFailed`, or `FileError`: Anchor peak missing, reference anchor failed, or spectrum file unreadable. Requires manual intervention or fallback to previous run. |

### 8.5 The Interactive Intervention Dialog & Live Floating Canvas HUD
Clicking **Intervene** (or double-clicking a cell in the run-by-run matrix) minimizes the bulky manager window, loads the target spectrum onto the main canvas zoomed directly to the anchor region, and opens the high-resolution diagnostic dialog:

```
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                        Inspect & Tune Fit — Run: run0042.spe, Detector #01 (0.50 keV/ch)               │
├────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Current Status: INTERVENE (High χ²)  |  Nominal: 0.50 keV/ch  |  Gain: 0.499820  |  Cutoff: ≤ 5.0      │
├────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Peak Anchors, Flat Background Baseline & Parameter Locks                                               │
│ ┌───────────────┬─────┬──────────────┬──────────────────┬───────────┬──────────────┬──────────┬──────┐ │
│ │ Action        │ Use │ Energy (keV) │ Centroid (ch) 🔒 │ Shift (Δ) │ Window (±ch) │ FWHM (ch)│ Base │ │
│ ├───────────────┼─────┼──────────────┼──────────────────┼───────────┼──────────────┼──────────┼──────┤ │
│ │ 🔄 Refit Peak │ [✓] │ 1173.24±0.05 │ [ 2346.12 ] [🔓] │ +0.42 ch  │ [  12.0  ]   │ [ 4.20 ] │ [45] │ │
│ │ 🔄 Refit Peak │ [✓] │ 1332.49±0.05 │ [ 2664.88 ] [🔒] │ -0.12 ch  │ [  14.0  ]   │ [ 4.50 ] │ [38] │ │
│ └───────────────┴─────┴──────────────┴──────────────────┴───────────┴──────────────┴──────────┴──────┘ │
├────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Live Recalibration Preview: Gain (a1) = 0.499981 | Offset (a0) = -0.04 ch | χ²/ndf = 1.18 (GOOD)       │
├────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ [🔄 Redraw Canvas]  [👁️ Minimize to View Canvas]   [✅ Apply Adjusted Fit]  [⚠️ Force Accept]  [Done]   │
└────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

#### The Live Floating Canvas HUD Inspector (`QZoomHUD`)
During intervention, NuTrackN automatically activates the **Floating Canvas HUD Inspector** (`QZoomHUD`), projecting a real-time, high-magnification peak fit window ($360 \times 230\text{ px}$) directly over the workspace:

- **Instant Parameter Feedback**: Changing any parameter (Centroid, FWHM, Window $\pm\text{ch}$, Baseline) or clicking **🔄 Refit Peak** instantly re-samples and updates the HUD's zoomed spectrum histogram, Gaussian fit curve (orange), chord-SNIP background baseline (dashed line), and centroid reticle marker.
- **Dedicated Anchor Identity & Fit Badge**: The top bar provides a collision-proof dual-zone header displaying the anchor identity (`🔍 Anchor Peak #5 (1054.3 keV)`) on the left and a compact goodness-of-fit badge (`FWHM 4.6 ch • χ² 1.12`) on the right. A subheader readout provides real-time channel coordinate, calibrated energy, and peak counts (`Ch 2107 (1054.13 keV) | 54 cts`).
- **Row & Main-Window Selection**: Clicking any row or cell in the intervention table immediately switches the HUD focus to that peak. Selecting anchor lines in the main Run-by-Run Manager's anchor table also previews the peak on the HUD when a spectrum is active on the ROOT canvas.
- **Non-Intrusive Smart Positioning**: The HUD utilizes `Qt::ToolTip | Qt::FramelessWindowHint` with `Qt::WA_ShowWithoutActivating` so it never steals keyboard focus while you type values into spinboxes. It automatically positions itself adjacent to the intervention dialog (flipping left if near screen borders) and tracks along when the dialog is moved or resized.
- **Peek Mode & Automatic Cleanup**: Clicking **👁️ Minimize to View Canvas** hides the HUD temporarily so you can view the entire ROOT canvas unobstructed; restoring the dialog re-displays the HUD. Upon accepting or canceling intervention, the HUD cleanly hides and restores its default AutoFit Target mode.

#### Table Controls & Calibration Actions
- **Shift Indicator Badge ($\Delta\text{ch}$)**: Shows the real-time difference between the current centroid and nominal position ($c - c_0$). Color-coded green ($< 2$ ch), orange ($2 - 5$ ch), and red ($> 5$ ch).
- **Search & Fit Window ($\pm\Delta\text{ch}$)**: Sets the exact half-window around the centroid used for baseline sampling and Gaussian fitting.
- **Parameter Locking (`FitFixedParams`)**: Click **[🔓 / 🔒]** next to Centroid, FWHM, or Baseline to lock that parameter during refitting while allowing other free parameters to adjust.
- **Live Recalibration Preview**: Instantly recalculates the energy calibration polynomial, residual deviation $s_{\text{res}}$, and calibration line $\chi^2/\text{ndf}$ across all checked anchor lines.
- **Action Buttons**:
  - **✅ Apply Adjusted Fit (Mark GOOD)**: Saves the tuned parameters and recomputed calibration into the detector trajectory and updates the matrix cell to green.
  - **⚠️ Force Accept As-Is**: Overrides strict $\chi^2$ cutoff thresholds, adopting the current parameters and marking the run as good.
  - **❌ Reject (Make it FAIL)**: Discards the run as an unrecoverable failure and sets matrix status to red.

### 8.6 High-Statistics Chunking & Hardware Jump-Aware Smoothing
When analyzing low-cross-section transitions or short beam runs, individual run spectra may lack sufficient counting statistics for stable peak fitting. NuTrackN provides a two-stage Pass 2 refinement system:

#### 1. Hardware Jump Boundary Detection
Hardware gain jumps (e.g., preamplifier resetting, high-voltage adjustments, or NIM crate thermal steps) produce abrupt gain discontinuities. NuTrackN automatically identifies these steps:
- **Cut (`%`)**: Configurable threshold (default `0.035 %`, corresponding to $|\Delta a_1| / a_1 \ge 0.00035$ or $\sim 1.4$ channels at channel 4000).
- Runs separated by jumps greater than this cut are partitioned into distinct, isolated segments. Neither chunking nor smoothing is permitted to cross jump boundaries, preserving true physical gain shifts.

#### 2. Pass 2 Raw Spectrum Chunking
Clicking **⚡ Re-Calibrate with Chunks** groups consecutive runs within each jump-bounded segment into chunks of size $K$ (configured via **Chunk: [ K ] runs**):
- **Raw Histogram Summing**: The engine accumulates raw spectra channel-by-channel across the chunk runs, multiplying peak statistics by $K$ and reducing statistical centroid variance by $\sqrt{K}$.
- **Chunk Max $\chi^2$ Cutoff**: Because Poisson uncertainty $\sigma = \sqrt{N}$ increases the reduced $\chi^2/\text{ndf}$ for high-count composites (e.g. from $\sim 2$ up to $\sim 15 - 30$ despite sub-channel centroid accuracy), a dedicated **Chunk Max $\chi^2$** spinbox (default `25.0`) prevents false rejections of high-statistics chunk fits.
- **Linear Center-Interpolation**: Fitted calibration parameters from chunk centers are smoothly interpolated across the constituent runs, eliminating non-physical staircase steps.
- **Chunk Intervention**: If a chunk composite fit fails or exceeds the cutoff, its runs are flagged as `INTERVENE` (orange). Double-clicking any constituent run opens the intervention dialog directly on the composite chunk spectrum, and applying or force-accepting the fit synchronously calibrates all runs in that chunk.

#### 3. Moving Window Smoothing (`Smooth a1:`)
To eliminate residual statistical jitter while preserving the underlying continuous gain drift:
- Check **Smooth a1:** in the inspector bar and select the moving average **Window** (e.g., 3 to 31 runs, odd).
- With **Preserve Jumps** enabled, smoothing applies a Gaussian-weighted moving filter strictly within continuous hardware segments, stopping dead at jump boundaries.
- The drift inspection canvas live-renders both the raw Pass 1 gain points (faint green) and the finalized/smoothed chunk trajectory (solid gold curve). Both `.cal` and `.ucal` exports automatically incorporate the smoothed parameters when this option is checked.

### 8.7 Export Formats: Legacy GASP .cal vs. Extended Multi-Order .ucal
Once all runs are validated, export calibrations locally or directly to a remote SSH directory:
1. **Legacy GASP Format (`.cal`)**:
   Standard GASPware / Xtrackn / GSORT format (can be exported as a combined file or per-run `.cal` files):
   ```text
   ! GASP Format Calibration File
   ! Run    Det    a0 (keV)       a1 (keV/ch)
   1        1      -0.512400      0.499821
   2        1      -0.511850      0.499834
   ```
2. **Extended Multi-Order Uncertainty Format (`.ucal`)**:
   Fixed-column tabular ASCII record containing the polynomial order (`NCoeff`), coefficients ($a_0, a_1, a_2$), parameter uncertainties ($\sigma_{a_0}, \sigma_{a_1}, \sigma_{a_2}$), intercept-slope covariance $\text{cov}(a_0, a_1)$, residual standard error $s_{\text{res}}$ (in keV), reduced $\chi^2/\text{NDF}$, and convergence status string (see [Section 14.2](#142-file-format-specifications) for the exact column schema).

---

## 9. 2D Coincidence Matrix Analysis

### 9.1 Foundations of Gamma-Gamma Coincidence Matrices
In experimental nuclear decay and heavy-ion reaction studies, detector arrays record pairs of gamma rays detected in prompt coincidence ($\Delta t < 20\text{ ns}$). These events form a 2D matrix $M(E_1, E_2)$ where each cell $(i, j)$ represents the number of coincidence events between channel $i$ and channel $j$.

NuTrackN reads both raw and symmetrized compressed coincidence matrices (`.cmat` and `.mat` formats).

### 9.2 Inspecting Matrices (`Q` & `MatrixDialog`)
- **`Q`**: Displays the **Total Projection** of the active compressed matrix onto the 1D canvas:
  $$P_{total}(E_2) = \sum_{E_1} M(E_1, E_2)$$
- Click the **`CM`** button or press **`DQ`** to open the 2D Matrix Inspector.

### 9.3 Coincidence Gating (`CW`) & Background Subtraction (`DW`)
To isolate gamma cascades feeding or depopulating a specific nuclear level:
1. Place coincidence **Gate Markers** (**`W`**) defining the energy window $[E_{gate\_min}, E_{gate\_max}]$ across the gating photopeak.
2. Press **`CW`** (Coincidence Window cut): NuTrackN slices through the 2D matrix:
   $$S_{gate}(E_2) = \sum_{E_1 = E_{gate\_min}}^{E_{gate\_max}} M(E_1, E_2)$$
3. **Background-Subtracted Gated Spectra (`DW`)**:
   Place background slices on both sides of the gating peak. NuTrackN computes the net coincidence spectrum:
   $$S_{net}(E_2) = S_{gate}(E_2) - \frac{A_{gate}}{A_{bg}} \cdot S_{bg}(E_2)$$
   completely eliminating random Compton coincidences and cleanly revealing the true coincident cascade.

---

## 10. Scripting, Macros & Automation

### 10.1 Graphical Interface Focus & Terminal Status Notice

> [!IMPORTANT]
> **Terminal & Batch Operation Notice:**  
> NuTrackN's development currently focuses on intuitive, user-friendly graphical dialog windows, interactive canvas controls, and real-time visual feedback. Consequently, **program operation through the terminal (either the system terminal / stdin or typing into the in-built bottom console) is not currently functional for command input.**
> 
> The in-built console at the bottom of the canvas operates exclusively as a **read-only output logger**, streaming calculation reports, marker moment tables, fit parameters, and command lists.
> 
> Full program operation through the terminal and headless batch execution (processing spectra and calibration pipelines with no GUI) will be the topic of a subsequent upgrade.

In the current release, automation and command sequencing are performed either via:
1. **Interactive Spectrum Canvas Hotkeys**: Executing single- and two-character command tokens directly on the active canvas (e.g., `L`, `CI`, `CJ`, `AG`, `E`, `FX`, `K`, `AK`, `AR`).
2. **The Macro Manager Dialog (`DM`)**: Storing, configuring, cycling, and executing multi-token automation sequences across slots `0` through `9`.

### 10.2 Supported Automation Tokens & Command Strings
Command strings configured in the **Macro Manager (`DM`)** accept authentic Xtrackn tokens separated by spaces, commas, or semicolons (e.g., `*1; FF; CP; CJ` or `FX; FY; L; =`):
- **Batch Stepping & Spectrum Navigation Tokens**:
  - **`*1` / `N+` / `N`**: Step to the next detector / spectrum buffer.
  - **`*2` / `N-`**: Step to the previous detector / spectrum buffer.
  - **`*3` / `R+` / `NR`**: Step to the next experimental run.
  - **`*4` / `R-` / `PR`**: Step to the previous experimental run.
- **Fitting & Analysis Tokens**:
  - **`CB`**: Fit linear background across active `B` markers.
  - **`CI` / `CJ`**: Integrate spectrum region (CI without background, CJ with linear background subtraction).
  - **`AG` / `AJ`**: Execute automatic Gaussian peak fit or automatic ROI integration at the cursor.
  - **`CP` / `MP` / `ZP`**: Execute automated peak search (`CP`), show peak labels (`MP`), or clear peak markers (`ZP`).
  - **`OR` / `DR` / `SSH`**: Open the Remote Data / SSH Session Manager.
- **Nested Macro Calls**: Including a digit `0`–`9` inside a command string invokes that sub-macro (up to a recursion depth of 10).

### 10.3 Numeric Macros (0 through 9) & Repetition Cycles
Assign frequently repeated command sequences to slots `0` through `9`:
- **`0..9`**: Execute stored macro `n` **once** (`1x`).
- **`D` + `0..9`**: Define a macro sequence interactively (e.g., `D1` followed by `*1; FF; CP; CJ`).
- **`C` + `0..9`**: Cycle stored macro `n` in a loop for `#Cicli` iterations (prompts for cycle count if not preset).
- **`M` + `0..9`**: Print the stored macro string and default cycle count in the output console.
- **`Z` + `0..9`**: Clear macro `n`.

### 10.4 The Macro Manager Dialog (`DM`)
Press **`DM`** (or click the **Macros** button) to open the graphical Macro Manager (`Automatic Command Strings & Macros (Block 4)`):

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                 Automatic Command Strings & Macros (Block 4)                         │
├──────┬────────────────────┬────────┬──────────────────────┬──────────────────────────┤
│ Slot │ Command String     │ Cycles │ Description / Note   │ Actions                  │
├──────┼────────────────────┼────────┼──────────────────────┼──────────────────────────┤
│  #0  │ CP; CG;            │   1    │ Auto-find & fit peak │ [Run (1x)] [Cycle] [Clear]│
│  #1  │ *1; CI; DF;        │  24    │ Integrate all dets   │ [Run (1x)] [Cycle] [Clear]│
│  #2  │ FX; FY; =;         │   1    │ Reset full display   │ [Run (1x)] [Cycle] [Clear]│
│ ...  │ ...                │  ...   │ ...                  │ [Run (1x)] [Cycle] [Clear]│
├──────┴────────────────────┴────────┴──────────────────────┴──────────────────────────┤
│ [ Load from File... ] [ Save to File... ]                 [ Apply ]  [ OK ]  [ Close ]│
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- Configure sequence strings, repetition loop counts (`1`–`10000`), and laboratory notes for all 10 slots (`#0`–`#9`), with an embedded **Command Syntax & Tokens Reference** cheat sheet at the bottom of the dialog.
- Click **[Run (1x)]** to execute a slot once, **[Cycle]** to run it for the configured number of cycles, or **[Clear]** to erase the slot.

### 10.5 Saving, Loading & Distributing Macro Files (`.mac`)
Click **Save to File...** or **Load from File...** (supporting both local disk and remote SSH directories via `NuFileDialog`) to export or import all macro definitions as a pipe-delimited `.mac` file (`ID|COMMAND_STRING|CYCLES|DESCRIPTION`; see [Section 14.2](#142-file-format-specifications)). Share standardized analysis scripts across research groups to ensure reproducible data reductions.

---

## 11. Aesthetic Customization & Theme Management

### 11.1 Built-in Themes
Click the **Design** button in the lower toolbar to open the Appearance Manager:
1. **Modern Dark (Deep Space Cyan)**: Sleek Obsidian/Navy dark mode (`#0f172a`), DejaVu Sans typography, neon cyan accents, and high-visibility pastel spectrum curves. Optimized for dark experimental control rooms.
2. **Modern Light (Crisp Clean)**: Pristine slate-light surface (`#f1f5f9`), white canvas background, and publication-ready dark curves.
3. **Classic (Legacy Xtrackn)**: Authentic slate-gray UI (`#708090`), Times New Roman typography, high-contrast cyan output console, and classic primary spectrum colors.

### 11.2 Customizing Palette Colors, Gridlines & Typography
- **Spectrum Curves**: Customize individual foreground colors and line thicknesses for Buffers 1 through 9.
- **Marker Colors**: Independently configure colors for Background (`B`), Integral (`I`), Range (`R`), Gauss (`G`), and Gate (`W`) markers.
- **Typography**: Select system fonts and point sizes for canvas readouts, status bars, and dialogs.

### 11.3 Theme Import and Export (`.nugasp-theme`)
- **Export Theme**: Click **`Export Theme...`** to save your customized color palette, fonts, and UI tokens into a portable `.nugasp-theme` JSON file.
- **Import Theme**: Click **`Import Theme...`** to load a colleague's custom theme file.

---

## 12. End-to-End Analysis Cookbook Workflows

### 12.1 Deconvoluting an Overlapping Multiplet (`R` → `G` → `CV`)
Please write me

### 12.2 Batch Integrating Across Detectors with a Macro (`*1` + `CJ`)
Please write me

### 12.3 Correcting Gain Drift Across 100+ In-Beam Runs (`AR`)
Please write me

---

## 13. Troubleshooting & Frequently Asked Questions (FAQ)

### 13.1 Zoom & Marker Placement Issues (`E`, `CI`, `CJ`)
Please write me

### 13.2 Resolving `INTERVENE` and `FAILED` States in Run-by-Run Alignment
Please write me

### 13.3 Low-Statistics Peak Fitting & Baseline Instabilities
Please write me

---

## 14. Appendices & Technical Specifications

### 14.1 Master Keyboard Command & Shortcut Reference

#### 📂 File, Buffer & Run/Detector Stepping Operations
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`N` / `N+` / `*1`** | **Next Spectrum / Det** | Open or step to the next 1D spectrum / detector |
| **`N-` / `*2`** | **Prev Spectrum / Det** | Step to the previous 1D spectrum / detector |
| **`*3` / `R+` / `NR`** | **Next Run** | Increment experimental run number |
| **`*4` / `R-` / `PR`** | **Previous Run** | Decrement experimental run number |
| **`OR` / `DR` / `SSH`** | **Remote Data (SSH)** | Open Remote Data Acquisition & SSH Session Manager |
| **`OS`** | **Output Spectrum** | Export current spectrum data to ASCII / CSV |
| **`O=`** | **Output Plot** | Generate vector PostScript / PDF of current view |
| **`Ctrl + →`** | **Add Column** | Add an additional spectrum display column to canvas |
| **`Ctrl + ←`** | **Delete Column** | Remove last spectrum display column from canvas |
| **`Ctrl + ↑`** | **Add Row** | Add an additional spectrum display row |
| **`Ctrl + ↓`** | **Delete Row** | Remove last spectrum display row |

#### 🔍 Canvas Navigation & Scaling
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`L`** | **Lin / Log Scale** | Toggle vertical axis between Linear and Logarithmic |
| **`E`** | **Expand / Zoom** | Zoom view to region between the last two `Spacebar` markers |
| **`X`** | **Expand at Cursor** | Zoom in centered at current mouse cursor position |
| **`FF` / `FS`** | **Full Display** | Reset view to full spectrum range (`FX` + `FY`) |
| **`FX`** | **Full X Range** | Reset horizontal axis to full energy/channel range |
| **`FY`** | **Full Y Range** | Reset vertical axis to visible maximum with headroom |
| **`<` / `>`** | **Pan Left / Right** | Shift visible spectrum window 75% to left or right |
| **`=`** | **Redraw / Refresh** | Refresh canvas display and clear temporary lines |
| **`FO` / `FU`** | **Set Y-Max / Y-Min** | Set vertical maximum or minimum to cursor count level |
| **`SX` / `SY`** | **Same X / Y Scale** | Synchronize X or Y axis range across all sub-windows |
| **`MZ`** | **Mark Zero** | Draw visual reference line at zero counts |

#### 📍 Markers & ROI Analysis
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`Spacebar`** | **Set Marker** | Place generic analysis/zoom marker at mouse pointer |
| **`B`** | **Background Marker** | Place background boundary marker (`Blue`, in pairs) |
| **`I`** | **Integral Marker** | Place peak integration boundary marker (`Yellow`, in pairs) |
| **`R`** | **Fit Range Marker** | Place Gaussian fitting range boundary marker (`Orange`) |
| **`G`** | **Gauss Marker** | Place Gaussian peak centroid marker (`Pink`) |
| **`W`** | **Gate Marker** | Place coincidence energy gate marker (`Purple`) |
| **`[` / `]`** | **Left / Right ROI** | Convenient left and right boundary placement |
| **`ZA`** | **Clear All Markers** | Delete all active markers across the canvas |
| **`ZB` / `ZI`** | **Clear Bg / Int** | Delete only background (`B`) or integral (`I`) markers |
| **`ZR` / `ZG` / `ZW`** | **Clear Fit / Gate** | Delete only range (`R`), Gaussian (`G`), or gate (`W`) markers |
| **`ZJ` / `ZV`** | **Clear Auto / Multi** | Delete auto-integration (`AJ`) or multi-fit (`CV`) markers |
| **`CB`** | **Fit Background** | Fit least-squares linear background across `B` marker pairs |
| **`CI`** | **Integrate (No Bg)** | Calculate gross area between `I` marker pairs (raw sum) |
| **`CJ`** | **Integrate (With Bg)** | Calculate net peak area, centroid & FWHM with linear bg subtraction |
| **`AJ`** | **Auto Integrate** | Automatically integrate peak under cursor with local background |
| **`DF` / `ZF`** | **Open / Close Log** | Designate or close output report logfile |

#### ⚡ Calibration & Peak Fitting
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`K`** | **2-Point Calibrate** | Fast 2-point calibration using last two markers |
| **`DK`** | **Define Calibration** | Open full polynomial energy calibration dialog |
| **`AK`** | **Auto Calibration** | Automated multi-source calibration ($^{60}\text{Co}, ^{137}\text{Cs}, ^{152}\text{Eu}$) |
| **`AR`** | **Run-by-Run Calib** | Automated in-beam drift alignment manager |
| **`DE`** | **Define Efficiency** | Open detector full-energy peak efficiency dialog |
| **`CP`** | **Calculate Peaks** | Automated peak search across visible spectrum range |
| **`MP` / `ZP`** | **Show / Clear Peaks** | Display or clear automated peak search (`CP`) annotations |
| **`CG`** | **Gaussian Fit** | Fit symmetric Gaussian peak(s) within range markers |
| **`CV`** | **Gauss + Step Fit** | Fit Gaussian with low-energy tail + ERFC Compton step |
| **`AG`** | **Auto Gauss Fit** | Automatically fit Gaussian peak under current cursor position |
| **`DG`** | **Define Fit Params** | Open peak fitting configuration (fixed/free FWHM, tails) |
| **`+` / `-`** | **Add / Remove Peak** | Add or remove peak centroid from fitting group |

#### 🔲 2D Matrices & Macros
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`Q`** | **Matrix Projection** | Display total projection of loaded matrix (`.cmat`) |
| **`CW`** | **Coincidence Cut** | Extract 1D coincidence slice within gate markers `W` |
| **`DW`** | **Define Cut Window** | Set coincidence gate boundaries and background slices |
| **`DQ` / `CM`** | **Matrix Dialog** | Select matrix file and background subtraction scheme |
| **`DM`** | **Macro Manager** | Open Macro & Command String Manager Dialog |
| **`0..9`** | **Run Macro (1x)** | Execute stored command sequence `0..9` once |
| **`D` + `0..9`** | **Define Macro** | Assign command sequence to numeric slot `0..9` |
| **`C` + `0..9`** | **Cycle Macro** | Loop stored command sequence `0..9` for $N$ cycles |
| **`M` + `0..9`** | **Show Macro** | Display stored command sequence in output console |
| **`Z` + `0..9`** | **Clear Macro** | Erase stored macro definition |
| **`H` / `?` / `F1`** | **Help Dialog** | Open in-app Help and Heritage documentation |

---

### 14.2 File Format Specifications

#### Calibration File (`.mcal`)
```text
! NuTrackN Energy Calibration File
CALIB_ORDER 2
A0  -0.452109
A1   0.499812
A2   1.0421e-07
FWHM_A0 1.2501
FWHM_A1 0.00184
```

#### Run-by-Run Legacy Calibration File (`.cal`)
```text
! GASPware Run-by-Run Drift Alignment
! Run    Det    a0 (keV)       a1 (keV/ch)
1        1      -0.512400      0.499821
2        1      -0.511850      0.499834
```

#### Run-by-Run Extended Uncertainty Calibration File (`.ucal`)
```text
# NuTrackN Extended Uncertainty Calibration File (.ucal)
# Format: Run Det NCoeff a0 a1 a2 sigma_a0 sigma_a1 sigma_a2 cov_a0_a1 s_res(keV) chi2/ndf Status
    1    1  2     -0.5124     0.499821   0.000000e+00     0.01420    0.0000112   0.000000e+00  -1.41200e-07     0.0312     1.142  OK
    2    1  2     -0.5119     0.499834   0.000000e+00     0.01510    0.0000118   0.000000e+00  -1.52300e-07     0.0345     1.205  OK
```

#### Macro File (`.mac`)
```text
# NuTrackN Macro Definitions (Block 4)
# Format: ID|COMMAND_STRING|CYCLES|DESCRIPTION
0|CP; CG|1|Auto-find and fit peak
1|*1; CI; DF|24|Batch integrate across 24 detectors
2|FX; FY; =|1|Reset full display
```

---

### 14.3 Coordinate System & Binning Conventions (0-Indexed Channels)
NuTrackN strictly enforces **0-indexed channel numbering** across all data structures, canvas cursors, HUD inspector readouts, and export routines:
- Channel 0 corresponds to the interval $[-0.5, +0.5)$ in continuous coordinate space.
- A 4096-channel spectrum spans channels $0$ through $4095$ (inclusive).
- When converting channels to energy:
  $$E(ch) = a_0 + a_1 \cdot ch + a_2 \cdot ch^2$$
  the channel value $ch$ evaluated is identical to the discrete MCA bin index.

---

### 14.4 System Requirements & Supported Platforms
- **Linux**: Ubuntu 20.04+, Debian 11+, Fedora 36+, Arch Linux, CentOS/RHEL 8+ (AppImage or Native CMake build).
- **macOS**: macOS 11.0 (Big Sur) through macOS 15.0+ (Sequoia) on Apple Silicon (M1/M2/M3/M4) or Intel Core 64-bit processors.
- **Hardware**:
  - Minimum: 2 GB RAM, 1 GHz Dual-Core CPU, $1280 \times 800$ display.
  - Recommended: 8 GB RAM, Quad-Core CPU, OpenGL 3.3+ acceleration, 4K High-DPI display.
