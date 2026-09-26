# NuTrackN (NuGASP) Official User Manual

**Version 1.0** — *Nuclear Gamma-Ray & Particle Spectroscopy Analysis Environment*  
**Architecture**: Qt5 + CERN ROOT Engine  
**Quick Help Shortcut**: Press **`H`** or **`?`** on the canvas at any time.

---

## Table of Contents
1. [Introduction & Overview](#1-introduction--overview)
   - [1.1 The Heritage of Xtrackn & GASPware](#11-the-heritage-of-xtrackn-and-gaspware)
   - [1.2 Historical Attributions & Acknowledgments](#12-historical-attributions--acknowledgments)
2. [Installation & Quick Start](#2-installation--quick-start)
3. [Data Input & Output (I/O)](#3-data-input--output-io)
4. [Canvas Navigation & Visualization](#4-canvas-navigation--visualization)
5. [Peak Integration & Region of Interest (ROI)](#5-peak-integration--region-of-interest-roi)
6. [Energy Calibration: Manual & Automated](#6-energy-calibration-manual--automated)
7. [Peak Fitting & Multiplet Deconvolution](#7-peak-fitting--multiplet-deconvolution)
8. [2D Coincidence Matrix Analysis](#8-2d-coincidence-matrix-analysis)
9. [Detector Efficiency Calibration](#9-detector-efficiency-calibration)
10. [Scripting, Macros & Automation](#10-scripting-macros--automation)
11. [Appendix & Technical Specifications](#11-appendix--technical-specifications)

---

## 1. Introduction & Overview

**NuTrackN** (part of the future **NuGASP** suite) is an interactive, high-performance nuclear spectroscopy analysis application designed for experimental nuclear physicists, gamma-ray spectroscopists, and radiation detection laboratories.

### 1.1 The Heritage of Xtrackn and GASPware
For decades, the classic Fortran/C software **Xtrackn** and the larger **GASPware suite** (developed across European and international heavy-ion and gamma-ray facilities like GASP, AGATA, and EUROBALL) served as the gold standard for high-throughput gamma-gamma matrix gating and spectrum analysis. Experimentalists developed deep muscle memory for its single-character and two-character command strings.

**NuTrackN** preserves **100% of authentic Xtrack keyboard commands**, while re-engineering the underlying platform into modern C++ with:

- **CERN ROOT Graphics Engine**: Sub-millisecond rendering of high-statistics histograms and logarithmic axes.

- **Modern Qt5 User Interface**: High-DPI screen support, responsive dialogs, live parameter tables, and curated dark/light themes.

- **2D Matrix Coincidence Viewer**: Fast slicing and projection of multi-gigabyte $\gamma$-$\gamma$ coincidence matrices (`.cmat`).

- **Autonomous Standalone Deployment**: 1-click execution without requiring users to configure compiler paths or install CERN ROOT manually.

- **An enlargement of the available developer base** due to migration to newer technologies

- **Extended maintenance and upgrade potential**

- **A comprehensive user manual**

### 1.2 Historical Attributions & Acknowledgments
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

## 2. Installation & Quick Start

### 2.1 Linux (1-Click AppImage)
NuTrackN is distributed as a completely self-contained Linux AppImage with embedded Qt5 and CERN ROOT runtimes:

# 1. Make executable
chmod +x NuTrackN-x86_64.AppImage

# 2. Add 'nutrackn' terminal command and Desktop Launcher (1-click)
./NuTrackN-x86_64.AppImage --install

# 3. Launch from anywhere in terminal
nutrackn

#(To remove: run `nutrackn --uninstall`).*

### 2.2 macOS (Drag-and-Drop DMG)
1. Download `NuTrackN-macOS-arm64.dmg` (Apple Silicon M1/M2/M3/M4) or `NuTrackN-macOS-x86_64.dmg` (Intel).
2. Open the disk image and drag **`NuTrackN.app`** into `/Applications`.
3. To enable the command line shortcut in Terminal:
   ```bash
   /Applications/NuTrackN.app/Contents/MacOS/nutrackn --install-cli
   ```

### 2.3 First Spectrum Walkthrough in 60 Seconds
1. Launch `nutrackn`.
2. Press **`N`** (or click the spectrum open button) and select your spectrum file (e.g. `152Eu_sample.spe`).
3. Press **`L`** to toggle between Linear and Logarithmic scale.
4. Hover the cursor over a photopeak; the **Live Peak Inspector Box** in the upper right dynamically displays centroid, channel, counts, and local FWHM.
5. Move cursor to the left of the peak and press **`[`**; move to the right and press **`]`**. Press **`E`** to zoom between the markers.
6. Press **`CJ`** to calculate net peak area with automatic linear background subtraction.

---

## 3. Data Input & Output (I/O)

### 3.1 Supported File Formats
NuTrackN automatically detects and parses all standard nuclear physics data formats:
- **Ortec Maestro (`.spe`, `.chn`)**: Standard multichannel analyzer ASCII and binary records.
- **Canberra Formats & Generic ASCII (`.txt`, `.dat`, `.csv`)**: Single-column counts or two-column (Channel, Counts).
- **Radware (`.spe`)**: Integer and floating-point Radware spectrum buffers.
- **CERN ROOT Histograms (`.root`)**: 1D histograms (`TH1D`, `TH1F`, `TH1I`) and 2D matrices (`TH2D`).
- **Compressed Coincidence Matrices (`.cmat`, `.mat`)**: GASPware symmetrized and raw 2D gamma coincidence matrices.

### 3.2 Loading Spectra
- **Keyboard Shortcut**: Press **`N`** to open the file selection dialog.
- **Command Line Launch**:
  ```bash
  nutrackn 137Cs.spe 60Co.spe
  ```
- **Opening Compressed Matrices**: Click the **`CM`** button or press **`DQ`** to open 2D matrices.

### 3.3 Multi-Spectrum Overlays & Buffers (1–9)
NuTrackN can hold up to 9 spectra in memory simultaneously:
- Press keys **`1` through `9`** to select the active foreground spectrum.
- Use **`Ctrl + →`** to add a new display column and **`Ctrl + ←`** to remove a column.
- Use **`Ctrl + ↑`** and **`Ctrl + ↓`** to add or remove display rows.
- Use **`SX`** / **`SY`** to enforce identical X or Y scales across all visible sub-canvases.

### 3.4 Exporting Processed Data
- **`OS`**: Export the active spectrum data (channel, calibrated energy, counts) to an ASCII format table.
- **`O=`**: Generate a publication-quality vector PostScript / PDF graphic of the current canvas view.
- **`DF`**: Designate an output report logfile for automated recording of peak integration areas.
- **`ZF`**: Close the active area calculation logfile.

---

## 4. Canvas Navigation & Visualization

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

### 4.3 Mouse Navigation Workflows
- **Rubber-band Box Zoom**: Left-click and drag a rectangular region over any spectrum feature to zoom directly into that bounding box.
- **Interactive Marker Dragging**: Click and drag existing markers along the spectrum to fine-tune integration limits.
- **Clickable Axis Labels**: Left-click or right-click the `X Min`, `X Max`, `Y Min`, or `Y Max` readout boxes on the bottom status bar to adjust limits in discrete steps.

### 4.4 Live Peak Inspector Box
Located in the upper right quadrant of the spectrum canvas, the **HUD Inspector** computes real-time statistics for whatever feature the cursor is hovering over:
- **Cursor Energy & Channel**: Exact interpolated calibration coordinate.
- **Counts / Bin**: Discrete histogram content.
- **Local Centroid**: Moment-calculated peak center within cursor proximity.
- **Local FWHM**: Instantaneous full-width at half-maximum.

### 4.5 Aesthetics & Design Customization
Click the **Palette Icon** on the toolbar to open the **Appearance Settings** dialog:
- **Presets Available**:
  1. **Classic (Legacy Xtrackn)**: Authentic Slate-gray UI (`#708090`), Times New Roman typography, high-contrast cyan command prompt, and classic primary spectrum colors.
  2. **Modern Dark (Deep Space Cyan)**: Sleek Obsidian/Navy dark mode (`#0f172a`), DejaVu Sans typography, neon cyan accents, and modern high-visibility pastel spectrum curves.
  3. **Modern Light (Crisp Clean)**: Pristine Nordic slate-light surface (`#f1f5f9`), white canvas background, and publication-ready dark curves.
- **Theme Import & Export**:
  - Click **`📤 Export Theme...`** to save all color and typography settings into a shareable `.nugasp-theme` JSON file.
  - Click **`📥 Import Theme...`** to load a colleague's custom theme into the preview window.

---

## 5. Peak Integration & Region of Interest (ROI)

### 5.1 Setting Boundary Markers
- **`Spacebar`**: Place generic analysis marker at mouse pointer.
- **`B`**: Place Background marker (`Blue`).
- **`I`**: Place Integration boundary marker (`Yellow`).
- **`[` and `]`**: Convenient left and right boundary placement.

### 5.2 Integration Commands & Mathematical Formulation
- **`CI` (Raw Area)**: Calculates gross integrated counts ($G$) between boundaries without background subtraction:
  $$G = \sum_{i = ch_{min}}^{ch_{max}} C_i$$
- **`CJ` (Net Area with Background Subtraction)**: Computes the net peak area ($N$) by subtracting an interpolated baseline:
  $$B = \frac{N_{channels}}{2} \cdot \left(\overline{C}_{left} + \overline{C}_{right}\right)$$
  $$N = G - B$$
- **Counting Statistics & Error Propagation**:
  The estimated statistical counting uncertainty $\sigma_{net}$ is computed as:
  $$\sigma_{net} = \sqrt{G + B \cdot \frac{N_{channels}}{2 \cdot N_{bg\_pts}}}$$
- **Peak Centroid Determination**:
  $$Ch_0 = \frac{\sum_{i} i \cdot (C_i - B_i)}{\sum_{i} (C_i - B_i)}$$

### 5.3 Marker Management & Clearing
- **`ZA`**: Erase all active markers from the canvas.
- **`ZB`**: Erase only background markers.
- **`ZI`**: Erase only integration markers.
- **`MI` / `MJ`**: Re-display and highlight integration markers.

---

## 6. Energy Calibration: Manual & Automated

### 6.1 Calibration Physics Formulation
Channel numbers ($ch$) are converted to physical gamma-ray energies ($E$, in keV) using a polynomial calibration model:
$$E(ch) = a_0 + a_1 \cdot ch + a_2 \cdot ch^2$$
where:
- $a_0$: Energy offset / zero-intercept (keV).
- $a_1$: Gain / dispersion slope (keV/channel).
- $a_2$: Non-linearity correction parameter (typically $< 10^{-6}$).

### 6.2 Manual Calibration Routine (`K` & `DK`)
1. **Fast 2-Point Calibration (`K`)**:
   - Place markers on two known photopeaks (e.g. 511.0 keV annihilation and 1274.5 keV $^{22}\text{Na}$).
   - Press **`K`** and type the two literature energies when prompted.
2. **Full Polynomial Calibration Dialog (`DK`)**:
   - Add multiple known peak centroids.
   - Enter literature energy values.
   - Click **Fit** to perform least-squares polynomial regression.
   - Inspect fit residuals ($\Delta E = E_{lit} - E_{calc}$).
   - Click **Save Calibration** to save the coefficients to a `.mcal` file.

### 6.3 Automated Radioisotope Calibration (`AK` / AutoCalib)
Press **`AK`** (or open the Auto Calibration Dialog):
1. Select your calibration source standard:
   - **$^{60}\text{Co}$**: 1173.228 keV, 1332.492 keV
   - **$^{137}\text{Cs}$**: 661.657 keV
   - **$^{133}\text{Ba}$**: 81.0, 276.4, 302.9, 356.0, 383.8 keV
   - **$^{152}\text{Eu}$**: Multi-line standard (121.8 keV through 1408.0 keV)
2. NuTrackN automatically performs peak searching, identifies characteristic isotope line spacings, matches peaks, and calculates calibration coefficients in one click.

---

## 7. Peak Fitting & Multiplet Deconvolution

### 7.1 Mathematical Response Models
Real semiconductor gamma detectors (such as High-Purity Germanium, HPGe, and Silicon detectors) exhibit slight peak asymmetry due to incomplete charge carrier collection. NuTrackN fits peaks using:

1. **Pure Gaussian (Symmetric)**:
   $$f(x) = A \cdot \exp\left( -\frac{(x - x_0)^2}{2\sigma^2} \right)$$
2. **Skewed Gaussian with Exponential Low-Energy Tail**:
   $$f(x) = \begin{cases} 
      A \cdot \exp\left( -\frac{(x - x_0)^2}{2\sigma^2} \right) & \text{for } x \ge x_0 - t \cdot \sigma \\
      A \cdot \exp\left( \frac{t^2}{2} + \frac{t(x - x_0)}{\sigma} \right) & \text{for } x < x_0 - t \cdot \sigma 
   \end{cases}$$
   where $t$ controls the tail joining point.

### 7.2 Fitting Workflow
1. Place **Fit Range** markers (**`R`**) around the peak or group of peaks.
2. Place **Gaussian centroid markers** (**`G`** or **`+`**) at the center of each component peak in the group.
3. Press **`CG`** for a standard Gaussian fit, or **`CV`** for a tail-corrected Gaussian + polynomial background fit.
4. The fit curve is overlaid in real time, and the parameter report displays:
   - Centroid ($x_0$) and calibrated energy ($E_0$).
   - Net Peak Area ($A$) and statistical uncertainty ($\sigma_A$).
   - Full Width at Half Maximum ($\text{FWHM} = 2.355 \cdot \sigma$).
   - Reduced goodness-of-fit: $\chi^2 / \text{NDF}$.

### 7.3 Multiplet Deconvolution
For closely overlapping doublets or multiplets:
- Press **`DG`** to open the **Fit Parameters Dialog**.
- Choose whether peak widths (FWHM) and tail parameters are free or **linked** across all components.
- Fixing relative FWHM values enables robust resolution of lines separated by less than $1\times\text{FWHM}$.

---

## 8. 2D Coincidence Matrix Analysis

### 8.1 2D Gamma-Gamma Matrix Basics
In nuclear decay and in-beam spectroscopy experiments, detector arrays record pairs of gamma rays detected in prompt coincidence ($\Delta t < 20\text{ ns}$). These events form a symmetric or asymmetric 2D matrix $M(E_1, E_2)$.

### 8.2 Inspecting Matrices (`Q` & `MatrixDialog`)
- **`Q`**: Displays the **Total Projection** of the active compressed matrix onto the 1D canvas (summing all coincidence rows).
- Click the **`CM`** button to open the 2D Matrix Inspector.

### 8.3 Gating & Slicing (`CW` & `DW`)
1. Place coincidence **Gate Markers** (**`W`**) defining the energy window of the feeding or depopulating transition:
   $$E_{gate\_min} \le E_1 \le E_{gate\_max}$$
2. Press **`CW`** (Coincidence Window cut): NuTrackN slices through the 2D matrix, extracting all gamma rays in prompt coincidence with your gate.
3. **Background-Subtracted Gated Spectra**:
   Use **`DW`** to define adjacent background gates on both sides of the peak. NuTrackN automatically computes:
   $$S_{net}(E) = S_{gate}(E) - f_{bg} \cdot S_{bg}(E)$$
   yielding pure coincidence cascades free of Compton background.

---

## 9. Detector Efficiency Calibration

### 9.1 Full Energy Peak Efficiency ($\epsilon_{FEP}$)
The absolute full-energy peak efficiency at energy $E_\gamma$ is determined from standard calibrated radioactive sources:
$$\epsilon(E_\gamma) = \frac{N_{net}(E_\gamma)}{A_{act} \cdot I_\gamma \cdot t_{live}}$$
where:
- $N_{net}$: Measured net peak counts.
- $A_{act}$: Source activity at reference date, corrected for elapsed decay time:
  $$A_{act} = A_0 \cdot \exp\left( -\frac{\ln 2 \cdot \Delta t}{T_{1/2}} \right)$$
- $I_\gamma$: Absolute emission probability per decay.
- $t_{live}$: Measurement live time (seconds).

### 9.2 Fitting Efficiency Curves (`DE`)
Press **`DE`** to open the **Efficiency Calibration Dialog**:
- Add measured data points $(E_i, \epsilon_i)$.
- Select the fitting model:
  - **Log-Log Polynomial**:
    $$\ln \epsilon = c_0 + c_1 \ln E + c_2 (\ln E)^2 + c_3 (\ln E)^3$$
  - **Dual-Regime Power Law**: Fitting low-energy absorption and high-energy pair production regimes.
- Click **Apply** to bind the efficiency calibration to the active session. Unknown sample activities are then calculated automatically from measured peak areas.

---

## 10. Scripting, Macros & Automation

### 10.1 Interactive Command Prompt
At the bottom of the main interface, the **Command Prompt** accepts direct text commands. Typing a command string executes immediately without requiring mouse clicks.

### 10.2 Numeric Macros (`0` through `9`)
Assign frequently repeated command sequences to single digits:
- **`D` + `0..9`**: Define a macro sequence (e.g. `D1` followed by `CP; CJ;`).
- **`C` + `0..9`**: Execute stored macro `n`.
- **`M` + `0..9`**: Print the stored macro string in the console.
- **`Z` + `0..9`**: Clear macro `n`.

### 10.3 Macro & Command String Manager (`DM`)
Press **`DM`** (or press `D` then `M`) to open the graphical **Macro Manager**:
- Edit, name, and test multi-line analysis scripts.
- Save scripts as `.mac` files for reproducible analysis across experimental campaigns.

---

## 11. Appendix & Technical Specifications

### 11.1 Calibration File Format (`.mcal`)
NuTrackN calibration files are plain ASCII text files structured as:
```text
! NuTrackN Energy Calibration File
CALIB_ORDER 2
A0  -0.452109
A1   0.499812
A2   1.0421e-07
FWHM_A0 1.2501
FWHM_A1 0.00184
```

### 11.2 System Requirements
- **Linux**: Ubuntu 20.04+, Debian 11+, Fedora 36+, Arch Linux, CentOS/RHEL 8+.
- **macOS**: macOS 11.0 (Big Sur) through macOS 15.0+ (Sequoia) on Apple Silicon (M1/M2/M3/M4) or Intel Core processors.
- **RAM**: 2 GB minimum (4 GB+ recommended for large coincidence matrices).
- **Display**: $1280 \times 800$ minimum resolution (fully optimized for 4K High-DPI screens).
