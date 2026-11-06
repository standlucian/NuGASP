# Run-by-Run Calibration Fitting Comparison: Legacy `Xtrackn` vs. `NuTrackN`

This document records the 7 architectural differences between legacy `Xtrackn` (`TRACKFITDEF` / `TRACKFIT` / `boundfit` in [`NuTrackN/Legacy/src/xtrack/trackn.F`](../NuTrackN/Legacy/src/xtrack/trackn.F)) and `NuTrackN` ([`NuTrackN/Sources/RunByRunEngine.cxx`](../NuTrackN/Sources/RunByRunEngine.cxx) & [`NuTrackN/Sources/TrackFitDialog.cxx`](../NuTrackN/Sources/TrackFitDialog.cxx)), along with the roadmap for porting `Xtrackn`'s coupled global fitting engine into `NuTrackN`.

---

## The 7 Major Differences

| # | Feature / Stage | Legacy `Xtrackn` (`DT` / `CT` in `trackn.F`) | `NuTrackN` (`RunByRunEngine.cxx`) | Impact on Summed Peak Widths |
| :- | :--- | :--- | :--- | :--- |
| **1** | **Core Fitting Architecture** | **Simultaneous Coupled Fit (`boundfit` + `TRFUN` + `TRDER`)**: One global Levenberg-Marquardt matrix (`Curfit1`) fits all spectrum bins across all peak regions simultaneously. | **Decoupled 2-Stage Fit (`fitAnchorPeakWithFixed` + `computeCalibration`)**: Fits each anchor peak in isolation with `TF1`, then runs weighted regression on $(\mu_i, E_i)$. | **Major**: In `NuTrackN`, statistical noise in a weak peak pulls its individual centroid $\mu_i$, which then pulls the run's gain/offset and smears the summed spectrum. |
| **2** | **Peak Centroid Constraint** | **Strictly Bound**: Peak $i$'s centroid is not a free parameter; it is evaluated directly from the global inverse polynomial at every iteration (`TRFUN` L5340): $\text{po}(E_i) = \sum_{k=0}^{N_E} p_k E_i^k$. | **Unconstrained per peak**: Each peak's $\mu_i$ floats freely inside `[fitMin, fitMax]` (or falls back to a raw $\pm 6\text{ ch}$ moment centroid if `TF1` fails). | **Major**: In `Xtrackn`, strong peaks directly stabilize weak peaks during the bin-level fit because all peaks share the same $p_k$ parameters. |
| **3** | **Peak Width ($\sigma$) Constraint** | **Strictly Bound & Two-Pass (`boundfit` L4989–L5000)**: Peak variance $\sigma^2(E_i) = \left|\sum w_k E_i^k\right|$ is locked to the global `WCAL` polynomial (`TRFUN` L5344). **Pass 1** freezes widths completely to fit positions; **Pass 2** unfreezes `WCAL` coefficients. | **Unconstrained per peak**: Every peak in every run fits its own independent $\sigma_i \in [0.2, 25.0]\text{ ch}$. | **High**: In low-count single runs, letting $\sigma_i$ float between $0.2$ and $25\text{ ch}$ allows the Gaussian to absorb background fluctuations or contaminant shoulders, shifting $\mu_i$. |
| **4** | **Background Model** | **Flat Only (`B1 = 0`)**: Linear slope is explicitly fixed to zero (`trpar(iioffs+2)=0`, `iwfree=0` at `boundfit` L4949–L4950). Overlapping peak windows are merged into a single region sharing one flat background (`TRSETREG` L5182–L5217). | **6-Parameter Model**: Gaussian + flat offset $B_0$ + linear slope $B_1(x-\mu)$ + skewed `erfc` Compton step $S$. | **High**: $B_1$ (slope) and $S$ (`erfc` step) are strongly covariant with the Gaussian centroid $\mu$. In noisy single runs, slope/step trade-offs systematically pull $\mu$. |
| **5** | **Fit Window Selection** | **Deterministic & Dynamically Centered (`TRSET` L5468–L5519)**: Window is strictly $\text{po}(E_i) \pm \text{FWHM}(E_i) \cdot \text{fwfact}$, recomputed at *every Levenberg-Marquardt step* as $\text{po}(E_i)$ moves. | **Data-Driven Local Valley Search**: Scans outward from `maxBin` for local minimum bins (`leftValley`, `rightValley`) to set `[fitMin, fitMax]`. | **Moderate–High**: In single runs, statistical dips change which bins are picked as `leftValley`/`rightValley` from run to run, jittering the fit range and baseline estimate. |
| **6** | **Whole-Spectrum Pre-Alignment** | **1024-ch Cross-Correlation (`gainfit` L4710–L4834)**: Compresses & smooths previous and current spectra to 1024 bins and fits a global shift+stretch `(sf0, sf1)` across the *entire spectrum* before fitting peaks. | **Per-Peak Trajectory Memory (`lastValidCentroids`)**: Tracks each peak's previous centroid individually; no whole-spectrum correlation pre-fit. | **Moderate**: `gainfit` catches sudden gain jumps coherently across the whole spectrum before peak fitting even starts. |
| **7** | **Bin-Center Convention** | **Adds $+0.5$ channel**: After `boundfit`, `TRACKFIT` L4496 & L4560 sets `pp = trpar(5*ii-1) + 0.5` before fitting $E(\text{ch})$, matching the `recal_gain` $+0.5$ bin convention. | **Integer bin centers**: `hTemp` uses `[fitMin - 0.5, fitMax + 0.5]`, so bin centers are integer channels `ch` without $+0.5$. | **Constant $0.5\text{ ch}$ offset**: Causes a half-channel systematic offset if mixed with legacy `Xtrackn` calibrations. |

---

## Mathematical Formulation of Legacy `Xtrackn` `boundfit`

### 1. Parameter Vector (`trpar`, length $5M + N_E + N_W$)
For $M$ reference peaks (`ntrpeak`) with reference energies $E_1, \dots, E_M$:
- For each peak $i \in \{1 \dots M\}$ at offset `iioffs = 5*(i - 1)`:
  - `trpar(iioffs + 1)`: $B_{0,r}$ — Flat background in region $r$ (**free** for the first peak in a region; shared if overlapping).
  - `trpar(iioffs + 2)`: $B_{1,r}$ — Background slope (**fixed to `0.0`**, `iwfree = 0`).
  - `trpar(iioffs + 3)`: $s^2_i = \sigma^2(E_i)$ — Gaussian variance in $\text{ch}^2$ (**dependent**, `iwfree = 0`, computed from `WCAL` polynomial).
  - `trpar(iioffs + 4)`: $\text{po}_i = \text{ch}(E_i)$ — Peak centroid in channels (**dependent**, `iwfree = 0`, computed from inverse `ECAL` polynomial).
  - `trpar(iioffs + 5)`: $A_i$ — Gaussian peak amplitude (**free**, `iwfree = 1`).
- Global inverse energy polynomial coefficients at `jestart = 5*M + 1` ($k = 0 \dots N_E - 1$, all **free**):
  $$\text{po}(E_i) = \sum_{k=0}^{N_E - 1} p_k E_i^k$$
- Global width-squared polynomial coefficients at `jwstart = 5*M + N_E + 1` ($k = 0 \dots N_W - 1$, **frozen in Pass 1**, **free in Pass 2**):
  $$\sigma^2(E_i) = \left|\sum_{k=0}^{N_W - 1} w_k E_i^k\right|$$

### 2. Bin-Level Model (`TRFUN`) & Analytical Jacobians (`TRDER`)
For a spectrum bin $x = \text{ich}$ lying inside fit region $r$ (spanning peaks $i \in [i_{1,r}, i_{2,r}]$):
$$y(x) = B_{0,r} + \sum_{i = i_{1,r}}^{i_{2,r}} A_i \exp\!\left(-\frac{(x - \text{po}_i)^2}{2 s^2_i}\right)$$
With $\Delta x_i = x - \text{po}_i$, $z_i = \frac{\Delta x_i^2}{2 s^2_i}$, and $G_i = \exp(-z_i)$:
- $\frac{\partial y}{\partial B_{0,r}} = 1$
- $\frac{\partial y}{\partial A_i} = G_i$
- $\frac{\partial y}{\partial \text{po}_i} = A_i G_i \frac{\Delta x_i}{s^2_i} \implies \frac{\partial y}{\partial p_k} = \sum_{i=i_{1,r}}^{i_{2,r}} \left(A_i G_i \frac{\Delta x_i}{s^2_i}\right) E_i^k$
- $\frac{\partial y}{\partial s^2_i} = A_i G_i \frac{\Delta x_i^2}{2 (s^2_i)^2} \implies \frac{\partial y}{\partial w_k} = \sum_{i=i_{1,r}}^{i_{2,r}} \text{sgn}\!\left(\sum w_m E_i^m\right) \left(A_i G_i \frac{\Delta x_i^2}{2 (s^2_i)^2}\right) E_i^k$

### 3. Two-Pass Levenberg-Marquardt (`Curfit1` + `TRSET`)
1. **Initialization (`boundfit` L4864–L4982)**:
   -Fits initial $p_k$ from $(\text{ENE}_i, \text{YFIT}_i)$ and initial $w_k$ from $(\text{ENE}_i, \sigma^2_i)$.
   - Builds non-overlapping regions `TRSETREG` of half-width $\text{fw}_i \cdot \text{fwfact}$, merging overlapping neighbors.
   - Estimates initial flat background $B_{0,r}$ from the lowest boundary counts and $A_i = y(\text{round}(\text{po}_i)) - B_{0,r}$.
2. **Pass 1 (`boundfit` L4991–L4993)**:
   - Sets `iwfree(jwstart .. jwstart+nwtrpar-1) = 0` (locks the width polynomial).
   - Runs `Curfit1` for up to 5 iterations. Inside `Curfit1`, `TRSET` is called after every parameter update to recompute $\text{po}_i$, $s^2_i$, and dynamically slide the fit window bounds `[itrc1, itrc2]` to stay centered on $\text{po}_i \pm \text{fw}_i \cdot \text{fwfact}$.
3. **Pass 2 (`boundfit` L4996–L4998)**:
   - Sets `iwfree(jwstart .. jwstart+nwtrpar-1) = 1` (unlocks the width polynomial).
   - Runs `Curfit1` for up to 5 more iterations to jointly refine widths, positions, amplitudes, and backgrounds.
4. **Forward Calibration Polynomial (`TRACKFIT` L4554–L4579)**:
   - Evaluates bound centroids $\text{pp}_i = \text{po}(E_i)$ (or $+0.5$ when using `Xtrackn` bin convention) and fits the forward polynomial $E(\text{ch}) = \sum_{k=0}^{N_E - 1} a_k \text{ch}^k$.

