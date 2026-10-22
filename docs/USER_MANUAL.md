# NuTrackN (NuGASP) Official User Manual

**Version 2.0** — *Nuclear Gamma-Ray & Particle Spectroscopy Analysis Environment*  
**Architecture**: Modern C++17 / Qt5 Core + CERN ROOT Scientific Graphics Engine  
**Quick Help Shortcut**: Press **`H`** or **`?`** (or **`F1`**) on the spectrum canvas at any time.

---

## Table of Contents
1. [Introduction, Heritage & Design Philosophy](#1-introduction-heritage--design-philosophy)
   - [1.1 The Heritage of Xtrackn & GASPware](#11-the-heritage-of-xtrackn--gaspware)
   - [1.2 Modern Architecture & Innovations](#12-modern-architecture--innovations)
   - [1.3 Historical Attributions & Acknowledgments](#13-historical-attributions--acknowledgments)
2. [Installation, Deployment & Quick Start](#2-installation-deployment--quick-start)
   - [2.1 Linux (1-Click AppImage)](#21-linux-1-click-appimage)
   - [2.2 macOS (Drag-and-Drop DMG for Apple Silicon & Intel)](#22-macos-drag-and-drop-dmg-for-apple-silicon--intel)
   - [2.3 Building from Source](#23-building-from-source)
   - [2.4 First Spectrum Walkthrough in 60 Seconds](#24-first-spectrum-walkthrough-in-60-seconds)
3. [Data Input, Output (I/O) & File Management](#3-data-input-output-io--file-management)
   - [3.1 Supported Nuclear Spectroscopy Formats](#31-supported-nuclear-spectroscopy-formats)
   - [3.2 Loading Spectra & Multi-Buffer Overlays (Buffers 1–9)](#32-loading-spectra--multi-buffer-overlays-buffers-19)
   - [3.3 Modern File Dialog (NuFileDialog) & Live Previews](#33-modern-file-dialog-nufiledialog--live-previews)
   - [3.4 Remote Data Acquisition & SSH Session Manager](#34-remote-data-acquisition--ssh-session-manager)
   - [3.5 Exporting Processed Data](#35-exporting-processed-data)
4. [Canvas Navigation, Scaling & Real-Time HUD](#4-canvas-navigation-scaling--real-time-hud)
   - [4.1 Display Scales & Vertical Adjustments](#41-display-scales--vertical-adjustments)
   - [4.2 Horizontal Panning & Zooming](#42-horizontal-panning--zooming)
   - [4.3 Mouse Navigation & Interactive Marker Dragging](#43-mouse-navigation--interactive-marker-dragging)
   - [4.4 Real-Time HUD Peak Inspector](#44-real-time-hud-peak-inspector)
5. [Peak Integration & Region of Interest (ROI) Analysis](#5-peak-integration--region-of-interest-roi-analysis)
   - [5.1 Setting Boundary Markers](#51-setting-boundary-markers)
   - [5.2 Mathematical Formulation of Gross & Net Peak Area](#52-mathematical-formulation-of-gross--net-peak-area)
   - [5.3 Counting Statistics & Error Propagation](#53-counting-statistics--error-propagation)
   - [5.4 Marker Management & Clearing](#54-marker-management--clearing)
   - [5.5 Output Report Logging](#55-output-report-logging)
6. [Standard Energy & Efficiency Calibration](#6-standard-energy--efficiency-calibration)
   - [6.1 Calibration Physics Formulation](#61-calibration-physics-formulation)
   - [6.2 Fast 2-Point Calibration Routine (K)](#62-fast-2-point-calibration-routine-k)
   - [6.3 Full Polynomial Calibration Dialog (DK)](#63-full-polynomial-calibration-dialog-dk)
   - [6.4 Automated Standard Radioisotope Calibration (AK)](#64-automated-standard-radioisotope-calibration-ak)
   - [6.5 Detector Full-Energy Peak Efficiency Calibration (DE)](#65-detector-full-energy-peak-efficiency-calibration-de)
7. [Advanced Peak Fitting & Multiplet Deconvolution](#7-advanced-peak-fitting--multiplet-deconvolution)
   - [7.1 Physical Peak Models (Gaussian & Low-Energy Tail)](#71-physical-peak-models-gaussian--low-energy-tail)
   - [7.2 Step Backgrounds: ERFC Compton Plateau & Median Sidebands](#72-step-backgrounds-erfc-compton-plateau--median-sidebands)
   - [7.3 Poisson Maximum Likelihood Estimation vs. Least-Squares](#73-poisson-maximum-likelihood-estimation-vs-least-squares)
   - [7.4 Dynamic Peak Extent, Valley Search & Physical Capping](#74-dynamic-peak-extent-valley-search--physical-capping)
   - [7.5 Multiplet Deconvolution & Parameter Linking/Locking](#75-multiplet-deconvolution--parameter-linkinglocking)
   - [7.6 Goodness of Fit & Residuals Inspection](#76-goodness-of-fit--residuals-inspection)
8. [In-Beam Run-by-Run Drift Alignment (Flagship Workflow)](#8-in-beam-run-by-run-drift-alignment-flagship-workflow)
   - [8.1 Physics of Thermal & Rate Gain Drifts in Long Experiments](#81-physics-of-thermal--rate-gain-drifts-in-long-experiments)
   - [8.2 Sequence Preparation & Anchor Peak Configuration](#82-sequence-preparation--anchor-peak-configuration)
   - [8.3 Automated Drift Tracking Engine](#83-automated-drift-tracking-engine)
   - [8.4 Status Interpretation: OK, INTERVENE, and FAILED](#84-status-interpretation-ok-intervene-and-failed)
   - [8.5 The Interactive Intervention Dialog](#85-the-interactive-intervention-dialog)
   - [8.6 Export Formats: Legacy GASP .cal vs. Extended Multi-Order .ucal](#86-export-formats-legacy-gasp-cal-vs-extended-multi-order-ucal)
9. [2D Coincidence Matrix Analysis](#9-2d-coincidence-matrix-analysis)
   - [9.1 Foundations of Gamma-Gamma Coincidence Matrices](#91-foundations-of-gamma-gamma-coincidence-matrices)
   - [9.2 Inspecting Matrices (Q & MatrixDialog)](#92-inspecting-matrices-q--matrixdialog)
   - [9.3 Coincidence Gating (CW) & Background Subtraction (DW)](#93-coincidence-gating-cw--background-subtraction-dw)
10. [Scripting, Macros & Automation](#10-scripting-macros--automation)
    - [10.1 Interactive Command Prompt](#101-interactive-command-prompt)
    - [10.2 Numeric Macros (0 through 9) & Repetition Cycles](#102-numeric-macros-0-through-9--repetition-cycles)
    - [10.3 The Macro Manager Dialog (DM)](#103-the-macro-manager-dialog-dm)
    - [10.4 Saving, Loading & Distributing Macro Files (.mac)](#104-saving-loading--distributing-macro-files-mac)
11. [Aesthetic Customization & Theme Management](#11-aesthetic-customization--theme-management)
    - [11.1 Built-in Themes (Modern Dark, Modern Light, Classic Xtrackn)](#111-built-in-themes-modern-dark-modern-light-classic-xtrackn)
    - [11.2 Customizing Palette Colors, Gridlines & Typography](#112-customizing-palette-colors-gridlines--typography)
    - [11.3 Theme Import and Export (.nugasp-theme)](#113-theme-import-and-export-nugasp-theme)
12. [Appendices & Technical Specifications](#12-appendices--technical-specifications)
    - [12.1 Master Keyboard Command & Shortcut Reference](#121-master-keyboard-command--shortcut-reference)
    - [12.2 File Format Specifications](#122-file-format-specifications)
    - [12.3 Coordinate System & Binning Conventions (0-Indexed Channels)](#123-coordinate-system--binning-conventions-0-indexed-channels)
    - [12.4 System Requirements & Supported Platforms](#124-system-requirements--supported-platforms)

---

## 1. Introduction, Heritage & Design Philosophy

**NuTrackN** (the flagship analysis environment of the **NuGASP** suite) is an interactive, high-performance nuclear spectroscopy application designed for experimental nuclear physicists, gamma-ray spectroscopists, and radiation detection laboratories.

### 1.1 The Heritage of Xtrackn & GASPware
For decades, the classic Fortran/C software **Xtrackn** and the larger **GASPware suite** (developed across European and international heavy-ion and gamma-ray facilities like GASP, AGATA, and EUROBALL) served as the gold standard for high-throughput gamma-gamma matrix gating and spectrum analysis. Experimentalists developed deep muscle memory for its single-character and two-character command strings.

**NuTrackN preserves 100% of authentic Xtrack keyboard commands**, while re-engineering the underlying platform into modern C++17 with:
- **CERN ROOT Graphics Engine**: Sub-millisecond rendering of high-statistics histograms and logarithmic axes.
- **Modern Qt5 User Interface**: High-DPI screen support, responsive dialogs, live parameter tables, and curated dark/light themes.
- **2D Matrix Coincidence Viewer**: Fast slicing and projection of multi-gigabyte $\gamma$-$\gamma$ coincidence matrices (`.cmat`).
- **Autonomous Standalone Deployment**: 1-click execution without requiring users to configure compiler paths or install CERN ROOT manually.
- **Run-by-Run Drift Alignment**: Automated in-beam gain tracking across hundreds of acquisition runs.

### 1.2 Modern Architecture & Innovations
NuTrackN couples the raw numerical computing performance of CERN ROOT with a high-responsiveness Qt5 event loop:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        NuTrackN Architecture                          │
├────────────────────────────────────────────────────────────────────────┤
│  Qt5 Presentation Layer (Modern Dark / Light / Classic Themes)         │
│  ├── QMainCanvas (OpenGL-accelerated ROOT embedded canvas)             │
│  ├── Live HUD Peak Inspector (Hover centroid & FWHM moments)          │
│  ├── RunByRunManager & Intervention Dialog (Drift alignment)           │
│  ├── RemoteClient (SSH tunnel & remote acquisition browsing)           │
│  └── MacroDialog (Slot 0–9 automation chains & cycling)               │
├────────────────────────────────────────────────────────────────────────┤
│  Nuclear Physics Computing Engine                                      │
│  ├── Poisson Likelihood & Adaptive Multiplet Minimizer (TMinuit)      │
│  ├── ERFC Compton Step & Median Sideband Background Modeling           │
│  ├── Coincidence Matrix Gating & 2D Projection Engine (.cmat)         │
│  └── Calibration Polynomial Regression Engine (Uncertainty Covariance)│
├────────────────────────────────────────────────────────────────────────┤
│  Data I/O & Storage Abstraction                                        │
│  └── Ortec (.spe, .chn), Radware, ROOT (.root), ASCII, GASP (.cmat)    │
└────────────────────────────────────────────────────────────────────────┘
```

### 1.3 Historical Attributions & Acknowledgments
The algorithms, design philosophies, and core workflows of NuTrackN stand upon decades of foundational work by the creators and maintainers of **GASPware** and **Xtrackn**, originally created for the GASP $\gamma$-ray spectrometer at the INFN Laboratori Nazionali di Legnaro (LNL) and Padova, Italy.

We gratefully acknowledge and credit:
- **Dino Bazzacco** (*INFN Sezione di Padova*): Main designer and author of the core GASP data analysis programs, including **TRACKN**, **CMAT**, and **GSORT**.
- **Călin A. Ur** (*INFN Padova / IFIN-HH Bucharest*): Co-author and core collaborator on the **GSORT** event-sorting engine.
- **Nicolae Mărginean** (*INFN LNL / IFIN-HH Bucharest*): Longtime maintainer and developer of GASPware, author of individual peak-width fitting routines in XTRACKN, asynchronous tape/data I/O, canvas enhancements, and cross-platform Unix/Linux/macOS ports.

Special thanks are also due to the authors of key open-source utilities and algorithms integrated into the original GASP ecosystem:
- **Fred Hucht** (*Universität Duisburg*): Creator of the **Ygl** library, which provided SGI GL graphics emulation under the X Window System.
- **Takuji Nishimura & Makoto Matsumoto**: Creators of the **Mersenne Twister** pseudorandom number generator used in event simulation and background generation.
- The authors at **M.S.I. Stockholm** and the **Niels Bohr Institute (NBI)** for the **laslib** PostScript plotting packages.

---

## 2. Installation, Deployment & Quick Start

### 2.1 Linux (1-Click AppImage)
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

### 2.2 macOS (Drag-and-Drop DMG for Apple Silicon & Intel)
1. Download `NuTrackN-macOS-arm64.dmg` (Apple Silicon M1/M2/M3/M4) or `NuTrackN-macOS-x86_64.dmg` (Intel Core).
2. Open the disk image and drag **`NuTrackN.app`** into `/Applications`.
3. To enable the command line shortcut in Terminal:
   ```bash
   /Applications/NuTrackN.app/Contents/MacOS/nutrackn --install-cli
   ```

### 2.3 Building from Source
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

### 2.4 First Spectrum Walkthrough in 60 Seconds
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
NuTrackN can hold up to 9 spectra in memory simultaneously:
- Press keys **`1` through `9`** to select the active foreground spectrum buffer.
- Use **`Ctrl + →`** to add a new display column and **`Ctrl + ←`** to remove a column.
- Use **`Ctrl + ↑`** and **`Ctrl + ↓`** to add or remove display rows.
- Use **`SX`** / **`SY`** to enforce identical X or Y scales across all visible sub-canvases.

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
- **`L`**: Toggle between **Linear** and **Logarithmic** Y-axis display.
- **`U` / `D`**: Vertically expand or compress the active histogram counts.
- **`FO`**: Set the vertical axis maximum (Y-Max) directly to the count level under the cursor.
- **`FU`**: Set the vertical axis minimum (Y-Min) directly to the count level under the cursor.
- **`MZ`**: Draw a reference baseline line at zero counts.

### 4.2 Horizontal Panning & Zooming
- **`E` (Expand)**: Zooms the horizontal axis to fit the region between the last two active markers.
- **`X`**: Centers the view around the current cursor location with 2× magnification.
- **`<` / `>`**: Pan the visible spectrum window 75% to the left or right.
- **`FF`**: Full display reset (both horizontal and vertical axes auto-scale to full dataset).
- **`FX` / `FY`**: Reset only the horizontal (energy/channel) or vertical (counts) scale.
- **`=`**: Redraw and refresh canvas.

### 4.3 Mouse Navigation & Interactive Marker Dragging
- **Rubber-band Box Zoom**: Left-click and drag a rectangular region over any spectrum feature to zoom directly into that bounding box.
- **Interactive Marker Dragging**: Left-click and drag any active marker along the spectrum curve to fine-tune integration or fitting limits interactively.
- **Clickable Axis Labels**: Left-click or right-click the `X Min`, `X Max`, `Y Min`, or `Y Max` readout boxes on the bottom status bar to adjust limits in discrete steps.

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
Given a photopeak bounded between channel $ch_{min}$ and channel $ch_{max}$, the total number of channels is:
$$N_{ch} = ch_{max} - ch_{min} + 1$$

1. **Gross Integrated Area ($G$)**:
   Calculated by the **`CI`** command without baseline subtraction:
   $$G = \sum_{i = ch_{min}}^{ch_{max}} C_i$$

2. **Background Baseline Estimation ($B$)**:
   The baseline under the peak is estimated from sideband regions of width $N_{bg}$ channels on the left and right of the peak:
   $$\overline{C}_{left} = \frac{1}{N_{bg}} \sum_{j=1}^{N_{bg}} C_{ch_{min} - j}, \quad \overline{C}_{right} = \frac{1}{N_{bg}} \sum_{j=1}^{N_{bg}} C_{ch_{max} + j}$$
   The total background counts $B$ trapped under the peak region is:
   $$B = \frac{N_{ch}}{2} \cdot \left(\overline{C}_{left} + \overline{C}_{right}\right)$$

3. **Net Peak Area ($N$)**:
   Calculated by the **`CJ`** command:
   $$N = G - B$$

### 5.3 Counting Statistics & Error Propagation
Assuming Poisson counting statistics for independent bin contents ($\sigma^2(C_i) = C_i$), the variance on the gross area is $\sigma^2(G) = G$. The variance of the estimated background is:
$$\sigma^2(B) = \left(\frac{N_{ch}}{2}\right)^2 \cdot \left[ \frac{\sigma^2(\overline{C}_{left})}{N_{bg}} + \frac{\sigma^2(\overline{C}_{right})}{N_{bg}} \right] = B \cdot \frac{N_{ch}}{2 \cdot N_{bg}}$$

The total uncertainty on the net peak area $\sigma_{net}$ is given by Gaussian error propagation:
$$\sigma_{net} = \sqrt{\sigma^2(G) + \sigma^2(B)} = \sqrt{G + B \cdot \frac{N_{ch}}{2 \cdot N_{bg}}}$$

**Centroid Determination**:
The net centroid $Ch_0$ is evaluated from the first moment of the background-subtracted distribution:
$$Ch_0 = \frac{\sum_{i = ch_{min}}^{ch_{max}} i \cdot (C_i - B_i)}{\sum_{i = ch_{min}}^{ch_{max}} (C_i - B_i)} = \frac{\sum_{i} i \cdot (C_i - B_i)}{N}$$

### 5.4 Marker Management & Clearing
- **`ZA`**: Erase all active markers from the canvas.
- **`ZB`**: Erase only background markers.
- **`ZI`**: Erase only integration markers.
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
The absolute full-energy peak efficiency at energy $E_\gamma$ is determined from standard calibrated radioactive sources:
$$\epsilon(E_\gamma) = \frac{N_{net}(E_\gamma)}{A_{act} \cdot I_\gamma \cdot t_{live}}$$
where:
- $N_{net}$: Measured net peak counts.
- $A_{act}$: Source activity at the measurement epoch, corrected for radioactive decay:
  $$A_{act}(t) = A_0 \cdot \exp\left( -\frac{\ln 2 \cdot \Delta t}{T_{1/2}} \right)$$
- $I_\gamma$: Absolute emission probability per decay.
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
2. **Load Run Sequence**: Select a sequence file or batch directory containing spectra for all experimental runs (e.g., `run0001.spe` through `run0250.spe`).
3. **Configure Anchor Peaks**:
   - Add 1 to 5 prominent, well-isolated reference gamma lines present throughout the beam time (e.g., 511.0 keV annihilation, 1460.8 keV $^{40}\text{K}$, or strong in-beam reaction transitions).
   - Enter physical energy ($E_{ref}$ in keV) and reference channel ($ch_{ref}$).
   - Set the **Search Window** ($\pm \Delta ch$, default: $\pm 5$ channels).

### 8.3 Automated Drift Tracking Engine
Click **Run Calibration**:
- The engine processes all runs asynchronously in background threads without freezing the user interface.
- For each run and detector, the engine searches within the $\pm 5$ channel window around the expected centroid, initializes peak parameters, and executes a Poisson-likelihood Gaussian + Compton step fit.
- The fitted centroid $ch_{fit}(t)$ is recorded alongside parameter uncertainties.

### 8.4 Status Interpretation: OK, INTERVENE, and FAILED
Each run in the sequence table is classified into one of three distinct operational states:

| Status Badge | Meaning | Description & Action Required |
| :--- | :--- | :--- |
| 🟢 **OK** | **Clean Convergence** | Peak was found within the window; fit converged with $\chi^2/\text{NDF} \le 2.0$ and expected physical FWHM. No manual action needed. |
| 🟠 **INTERVENE** | **Manual Review Advised** | Peak converged, but has moderate centroid drift, elevated $\chi^2$, or low statistics. Click **Intervene** to inspect and approve. |
| 🔴 **FAILED** | **Convergence Failure** | Peak not found, count rate too low, or minimizer did not converge. Requires manual intervention or fallback to previous run. |

### 8.5 The Interactive Intervention Dialog
Clicking **Intervene** on any flagged run opens the high-resolution diagnostic dialog:

```
┌────────────────────────────────────────────────────────────────────────┐
│               Run-by-Run Peak Fit Intervention                         │
├────────────────────────────────────────────────────────────────────────┤
│ Run: run0042.spe    Detector: Ge_01    Anchor: 1332.49 keV             │
├────────────────────────────────────────────────────────────────────────┤
│  [Spectrum Fit Canvas with Gaussian overlay & Background step]         │
│                                                                        │
│  Centroid Drift Visualizer:  [-5.0 ... ──[●]── ... +5.0]  (+1.84 ch)  │
├────────────────────────────────────────────────────────────────────────┤
│  Centroid (ch): [ 2666.04 ] [Lock]     FWHM (ch): [ 4.82 ] [Lock]      │
│  Baseline (cnt): [ 42.10 ] [Lock]     Slope:     [ -0.01 ] [Lock]      │
│                                                                        │
│  [ Exclude Peak ]    [ Re-Fit Peak ]    [ Accept & Next ]   [ Cancel ] │
└────────────────────────────────────────────────────────────────────────┘
```

- **Centroid Drift Visualizer**: A colored horizontal indicator displays the exact deviation of the peak from the reference channel.
- **Debounced Live Refit**: Adjusting the Centroid or FWHM spinboxes triggers an automatic refit after 400ms without requiring extra clicks.
- **Parameter Locking**: For low-statistics runs, click **[Lock]** next to Centroid or FWHM to hold that parameter constant while fitting background and amplitude.
- **Independent Slope & Baseline Controls**: Fine-tune baseline sideband levels independently to handle localized Compton edges.

### 8.6 Export Formats: Legacy GASP .cal vs. Extended Multi-Order .ucal
Once all runs are validated, click **Export Calibrations**:
1. **Legacy GASP Format (`.cal`)**:
   Standard 2-parameter format compatible with GASPware, Xtrackn, and GSORT:
   ```text
   ! GASP Format Calibration File
   ! Run    Det    a0 (keV)       a1 (keV/ch)
   1        1      -0.512400      0.499821
   2        1      -0.511850      0.499834
   ```
2. **Extended Multi-Order Format (`.ucal`)**:
   Modern JSON-compatible format containing higher-order polynomial coefficients ($a_0, a_1, a_2$), parameter covariance $\text{cov}(a_0, a_1)$, residual standard error $s_{res}$, and reduced $\chi^2$.

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

### 10.1 Interactive Command Prompt
At the bottom of the interface, the **Command Prompt** accepts direct text commands. Typing authentic Xtrack strings executes immediately without requiring mouse interaction:
- Type `L` and press Enter to toggle log scale.
- Type `CI` or `CJ` to integrate.
- Combine multi-command chains separated by semicolons (e.g., `FX; FY; L; =`).

### 10.2 Numeric Macros (0 through 9) & Repetition Cycles
Assign frequently repeated command sequences to single digits:
- **`D` + `0..9`**: Define a macro sequence (e.g., `D1` followed by `CP; CJ;`).
- **`C` + `0..9`**: Execute stored macro `n` through its configured repetition cycles.
- **`M` + `0..9`**: Print the stored macro string in the console.
- **`Z` + `0..9`**: Clear macro `n`.

### 10.3 The Macro Manager Dialog (`DM`)
Press **`DM`** (or click the **Macros** button) to open the graphical Macro Manager:

```
┌────────────────────────────────────────────────────────────────────────┐
│             Automatic Command Strings & Macros (Slots 0–9)             │
├────────────────────────────────────────────────────────────────────────┤
│ Slot │ Command String         │ Cycles │ Description / Note   │ Action │
├──────┼────────────────────────┼────────┼──────────────────────┼────────┤
│  0   │ CP; CG;                │   1    │ Auto-find & fit peak │ [Run]  │
│  1   │ CI; DF;                │   5    │ Integrate & log ROI  │ [Run]  │
│  2   │ FX; FY; =;             │   1    │ Reset full display   │ [Run]  │
│ ...  │ ...                    │  ...   │ ...                  │ [Run]  │
├────────────────────────────────────────────────────────────────────────┤
│ [ Load .mac Script ]   [ Save .mac Script ]   [ Clear All ]   [ Close ]│
└────────────────────────────────────────────────────────────────────────┘
```

- Configure sequence strings, repetition loop counts, and laboratory notes for all 10 slots.
- Click **[Run]** to trigger the macro directly from the dialog.

### 10.4 Saving, Loading & Distributing Macro Files (`.mac`)
Click **Save .mac Script** to save all 10 macro definitions to a plain ASCII `.mac` file. Share standardized analysis scripts across research groups to ensure reproducible data reductions.

---

## 11. Aesthetic Customization & Theme Management

### 11.1 Built-in Themes
Click the **Design** button in the lower toolbar to open the Appearance Manager:
1. **Modern Dark (Deep Space Cyan)**: Sleek Obsidian/Navy dark mode (`#0f172a`), DejaVu Sans typography, neon cyan accents, and high-visibility pastel spectrum curves. Optimized for dark experimental control rooms.
2. **Modern Light (Crisp Clean)**: Pristine slate-light surface (`#f1f5f9`), white canvas background, and publication-ready dark curves.
3. **Classic (Legacy Xtrackn)**: Authentic slate-gray UI (`#708090`), Times New Roman typography, high-contrast cyan command prompt, and classic primary spectrum colors.

### 11.2 Customizing Palette Colors, Gridlines & Typography
- **Spectrum Curves**: Customize individual foreground colors and line thicknesses for Buffers 1 through 9.
- **Marker Colors**: Independently configure colors for Background (`B`), Integral (`I`), Range (`R`), Gauss (`G`), and Gate (`W`) markers.
- **Typography**: Select system fonts and point sizes for canvas readouts, status bars, and dialogs.

### 11.3 Theme Import and Export (`.nugasp-theme`)
- **Export Theme**: Click **`Export Theme...`** to save your customized color palette, fonts, and UI tokens into a portable `.nugasp-theme` JSON file.
- **Import Theme**: Click **`Import Theme...`** to load a colleague's custom theme file.

---

## 12. Appendices & Technical Specifications

### 12.1 Master Keyboard Command & Shortcut Reference

#### 📂 File & Buffer Operations
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`N`** | **New Spectrum** | Open and load a new 1D spectrum into active window |
| **`OS`** | **Output Spectrum** | Export current spectrum data to ASCII / CSV |
| **`O=`** | **Output Plot** | Generate vector PostScript / PDF of current view |
| **`1` – `9`** | **Select Spectrum** | Switch active spectrum buffer / toggle overlay |
| **`Ctrl + →`** | **Add Column** | Add an additional spectrum display column to canvas |
| **`Ctrl + ←`** | **Delete Column** | Remove last spectrum display column from canvas |
| **`Ctrl + ↑`** | **Add Row** | Add an additional spectrum display row |
| **`Ctrl + ↓`** | **Delete Row** | Remove last spectrum display row |

#### 🔍 Canvas Navigation & Scaling
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`L`** | **Lin / Log Scale** | Toggle vertical axis between Linear and Logarithmic |
| **`E`** | **Expand / Zoom** | Zoom view to region between the last two markers |
| **`X`** | **Expand at Cursor** | Zoom in centered at current mouse cursor position |
| **`FF`** | **Full Display** | Reset view to full spectrum range (both X and Y) |
| **`FX`** | **Full X Range** | Reset horizontal axis to full energy/channel range |
| **`FY`** | **Full Y Range** | Reset vertical axis to full auto-scaled count height |
| **`<` / `>`** | **Pan Left / Right** | Shift visible spectrum window 75% to left or right |
| **`=`** | **Redraw / Refresh** | Refresh canvas display and clear temporary lines |
| **`FO` / `FU`** | **Set Y-Max / Y-Min** | Set vertical maximum or minimum to cursor count level |
| **`SX` / `SY`** | **Same X / Y Scale** | Synchronize X or Y axis range across all sub-windows |
| **`MZ`** | **Mark Zero** | Draw visual reference line at zero counts |

#### 📍 Markers & ROI Analysis
| Key / Command | Action | Description |
| :--- | :--- | :--- |
| **`Spacebar`** | **Set Marker** | Place generic analysis marker at mouse pointer |
| **`B`** | **Background Marker** | Place background boundary marker (`Blue`) |
| **`I`** | **Integral Marker** | Place peak integration boundary marker (`Yellow`) |
| **`R`** | **Fit Range Marker** | Place Gaussian fitting range boundary marker (`Orange`) |
| **`G`** | **Gauss Marker** | Place Gaussian peak centroid marker (`Pink`) |
| **`W`** | **Gate Marker** | Place coincidence energy gate marker (`Purple`) |
| **`[` / `]`** | **Left / Right ROI** | Convenient left and right boundary placement |
| **`ZA`** | **Clear All Markers** | Delete all active markers across the canvas |
| **`ZB` / `ZI`** | **Clear Bg / Int** | Delete only background or integral markers |
| **`ZG` / `ZW`** | **Clear Gauss / Gate** | Delete only Gaussian peak or coincidence gate markers |
| **`CI`** | **Integrate (No Bg)** | Calculate gross area between two markers (raw sum) |
| **`CJ`** | **Integrate (With Bg)** | Calculate net peak area with linear background subtraction |
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
| **`CG`** | **Gaussian Fit** | Fit symmetric Gaussian peak within range markers |
| **`CV`** | **Gauss + Step Fit** | Fit Gaussian with low-energy tail + ERFC Compton step |
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
| **`D` + `0..9`** | **Define Macro** | Assign command sequence to numeric key `0..9` |
| **`C` + `0..9`** | **Execute Macro** | Run stored command sequence `0..9` |
| **`M` + `0..9`** | **Show Macro** | Display stored command sequence in console |
| **`Z` + `0..9`** | **Clear Macro** | Erase stored macro definition |
| **`H` / `F1`** | **Help Dialog** | Open in-app Help and Heritage documentation |

---

### 12.2 File Format Specifications

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

#### Run-by-Run Calibration File (`.cal`)
```text
! GASPware Run-by-Run Drift Alignment
! Run    Det    a0 (keV)       a1 (keV/ch)
1        1      -0.512400      0.499821
2        1      -0.511850      0.499834
```

#### Macro File (`.mac`)
```text
! NuTrackN Macro Definitions
[Slot_0]
Command=CP; CG;
Cycles=1
Note=Auto-find and fit peak
[Slot_1]
Command=CI; DF;
Cycles=5
Note=Batch integrate
```

---

### 12.3 Coordinate System & Binning Conventions (0-Indexed Channels)
NuTrackN strictly enforces **0-indexed channel numbering** across all data structures, canvas cursors, HUD inspector readouts, and export routines:
- Channel 0 corresponds to the interval $[-0.5, +0.5)$ in continuous coordinate space.
- A 4096-channel spectrum spans channels $0$ through $4095$ (inclusive).
- When converting channels to energy:
  $$E(ch) = a_0 + a_1 \cdot ch + a_2 \cdot ch^2$$
  the channel value $ch$ evaluated is identical to the discrete MCA bin index.

---

### 12.4 System Requirements & Supported Platforms
- **Linux**: Ubuntu 20.04+, Debian 11+, Fedora 36+, Arch Linux, CentOS/RHEL 8+ (AppImage or Native CMake build).
- **macOS**: macOS 11.0 (Big Sur) through macOS 15.0+ (Sequoia) on Apple Silicon (M1/M2/M3/M4) or Intel Core 64-bit processors.
- **Hardware**:
  - Minimum: 2 GB RAM, 1 GHz Dual-Core CPU, $1280 \times 800$ display.
  - Recommended: 8 GB RAM, Quad-Core CPU, OpenGL 3.3+ acceleration, 4K High-DPI display.
