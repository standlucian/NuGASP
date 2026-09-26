# NuTrackN / Xtrackn - Laboratory Desk Cheat-Sheet

**Quick Help**: Press **`H`** or **`?`** (or **`F1`**) at any time on the spectrum canvas.

---

## 📂 1. Spectrum Input & Output (I/O)

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`N`** | **New Spectrum** | Open and load a new 1D spectrum into active window |
| **`OS`** | **Output Spectrum** | Export current spectrum data to ASCII / CSV |
| **`O=`** | **Output Plot** | Generate vector Postscript / PDF print of current view |
| **`1` – `9`** | **Select Spectrum** | Switch active spectrum buffer / toggle overlay |
| **`Ctrl` + `→`** | **Add Column** | Add an additional spectrum display column to canvas |
| **`Ctrl` + `←`** | **Delete Column** | Remove last spectrum display column from canvas |
| **`Ctrl` + `↑`** | **Add Row** | Add an additional spectrum display row |
| **`Ctrl` + `↓`** | **Delete Row** | Remove last spectrum display row |

---

## 🔍 2. Canvas Navigation & Scaling

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`L`** | **Lin / Log Scale** | Toggle vertical axis between Linear and Logarithmic display |
| **`E`** | **Expand / Zoom** | Zoom view to region between the last two markers |
| **`X`** | **Expand at Cursor** | Zoom in centered at current mouse cursor position |
| **`FF`** | **Full Display** | Reset view to full spectrum range (both X and Y) |
| **`FX`** | **Full X Range** | Reset horizontal axis to full energy/channel range |
| **`FY`** | **Full Y Range** | Reset vertical axis to full auto-scaled count height |
| **`<`** | **Pan Left** | Shift visible spectrum window 3/4 screen to the left |
| **`>`** | **Pan Right** | Shift visible spectrum window 3/4 screen to the right |
| **`=`** | **Redraw / Refresh** | Refresh canvas display and clear temporary lines |
| **`FO`** | **Set Y-Max** | Set vertical maximum to cursor/marker count level |
| **`FU`** | **Set Y-Min** | Set vertical minimum to cursor/marker count level |
| **`SX`** | **Same X Scale** | Synchronize X-axis range across all open sub-windows |
| **`SY`** | **Same Y Scale** | Synchronize Y-axis range across all open sub-windows |
| **`MZ`** | **Mark Zero** | Draw visual reference line at zero counts |

---

## 📍 3. Markers & Region of Interest (ROI)

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`Spacebar`** | **Set Marker** | Place a standard analysis marker at cursor position |
| **`B`** | **Background Marker** | Place background boundary marker (`Blue`) |
| **`I`** | **Integral Marker** | Place peak integration boundary marker (`Yellow`) |
| **`R`** | **Fit Range Marker** | Place Gaussian fitting range boundary marker (`Orange`) |
| **`G`** | **Gauss Marker** | Place Gaussian peak centroid marker (`Pink`) |
| **`W`** | **Gate Marker** | Place coincidence energy gate marker (`Purple`) |
| **`ZA`** | **Clear All Markers** | Delete all active markers across the canvas |
| **`ZB`** | **Clear Bg Markers** | Delete only background markers |
| **`ZI`** | **Clear Int Markers** | Delete only integral markers |
| **`ZG`** | **Clear Gauss Markers**| Delete only Gaussian peak markers |
| **`ZW`** | **Clear Gate Markers** | Delete only coincidence gate markers |

---

## 📊 4. Peak Integration & Area Calculation

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`CI`** | **Integrate (No Bg)** | Calculate gross area between two markers (raw sum) |
| **`CJ`** | **Integrate (With Bg)**| Calculate net peak area with linear background subtraction |
| **`CB`** | **Calculate Bg** | Calculate and display background baseline level |
| **`MI` / `MJ`** | **Show Int Markers** | Display / highlight integration markers on spectrum |
| **`DF`** | **Define File** | Open / select report output file for area calculations |
| **`ZF`** | **Close File** | Close active area calculation report file |

---

## ⚡ 5. Energy & Efficiency Calibration

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`K`** | **2-Point Calibrate** | Fast 2-point energy calibration using last two peak energies |
| **`DK`** | **Define Calibration** | Open full energy and FWHM polynomial calibration dialog |
| **`AK`** | **Auto Calibration** | Automated multi-source calibration ($^{60}\text{Co}$, $^{137}\text{Cs}$, $^{152}\text{Eu}$) |
| **`DT`** | **TrackFit Calib** | Recalibration using track polynomial fitting |
| **`DE`** | **Define Efficiency** | Open detector full-energy peak efficiency calibration dialog |

---

## 🎯 6. Peak Search & Fitting

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`CP`** | **Calculate Peaks** | Automated peak search across visible spectrum range |
| **`MP`** | **Show Peaks** | Display all detected peak centroids on screen |
| **`ZP`** | **Clear Peaks** | Clear peak buffer and erase peak indicators |
| **`CG`** | **Gaussian Fit** | Fit single or multi-Gaussian peak within range markers |
| **`CV`** | **Gauss + Bg Fit** | Fit Gaussian with charge-trapping low-energy tail + Bg |
| **`DG`** | **Define Gauss Params**| Open peak fitting configuration (fixed/free FWHM, tails) |
| **`+`** | **Add Peak** | Add peak centroid at marker position to fit group |
| **`-`** | **Remove Peak** | Remove nearest peak from current fitting group |

---

## 🔲 7. 2D Coincidence Matrix Analysis

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`Q`** | **Matrix Projection** | Display total projection of loaded compressed matrix (`.cmat`)|
| **`CW`** | **Coincidence Cut** | Extract 1D coincidence spectrum within gate markers `W` |
| **`DW`** | **Define Cut Window** | Set coincidence gate boundaries and background slices |
| **`DQ`** | **Define Matrix Mode**| Select matrix file and background subtraction scheme |
| **`MW`** | **Show Gate Markers** | Re-display active coincidence gate markers on projection |

---

## 🤖 8. Macros, Automation & Exiting

| Command / Key | Action | Description |
| :--- | :--- | :--- |
| **`DM`** | **Macro Manager** | Open Macro & Command String Manager Dialog (`D+M`) |
| **`D` + `0..9`** | **Define Macro String**| Assign command sequence to numeric key `0` through `9` |
| **`C` + `0..9`** | **Execute Macro** | Run stored command sequence `0` through `9` |
| **`M` + `0..9`** | **Show Macro** | Display stored command sequence in Command Prompt |
| **`Z` + `0..9`** | **Clear Macro** | Erase stored macro definition |
| **`Ctrl` + `C`** | **Quit NuTrackN** | Prompt confirmation dialog and exit cleanly |
