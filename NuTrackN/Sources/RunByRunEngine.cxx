#include "RunByRunEngine.h"
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "TH1F.h"
#include "TF1.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "Math/Minimizer.h"
#include "Math/Factory.h"
#include "Math/IFunction.h"

RunByRunEngine::RunByRunEngine(QObject *parent)
    : QObject(parent)
{
}

RunByRunEngine::~RunByRunEngine()
{
}

void RunByRunEngine::setConfig(const RunByRunConfig &config)
{
    m_config = config;
}

void RunByRunEngine::requestStop()
{
    m_stopRequested.store(true);
}

std::vector<RunCalibResult> RunByRunEngine::getAllFailures() const
{
    std::vector<RunCalibResult> failures;
    for (const auto &traj : m_trajectories) {
        for (const auto &res : traj.runResults) {
            if (res.status != RunCalibStatus::Success) {
                failures.push_back(res);
            }
        }
    }
    return failures;
}

//==============================================================================
// fitAnchorPeak
//==============================================================================
//==============================================================================
// fitAnchorPeak & fitAnchorPeakWithFixed
//==============================================================================
FittedAnchor RunByRunEngine::fitAnchorPeak(const std::vector<double> &spectrum,
                                          double centerCh, double windowCh,
                                          double minCounts, double energy)
{
    FitFixedParams fixed;
    return fitAnchorPeakWithFixed(spectrum, centerCh, windowCh, minCounts, energy, fixed);
}

FittedAnchor RunByRunEngine::fitAnchorPeakWithFixed(const std::vector<double> &spectrum,
                                                  double centerCh, double windowCh,
                                                  double minCounts, double energy,
                                                  const FitFixedParams &fixed)
{
    FittedAnchor result;
    result.energy = energy;
    result.isValid = false;

    const int totalBins = static_cast<int>(spectrum.size());
    if (totalBins <= 10) return result;

    if (fixed.fixCentroid && fixed.fixedCentroid > 0.0) {
        centerCh = fixed.fixedCentroid;
    }

    const int lowCh = std::max(1, static_cast<int>(std::floor(centerCh - windowCh)));
    const int highCh = std::min(totalBins - 2, static_cast<int>(std::ceil(centerCh + windowCh)));
    if (lowCh >= highCh) return result;

    // 1. Search for highest bin in search window (or use fixed centroid)
    int maxBin = lowCh;
    if (fixed.fixCentroid && fixed.fixedCentroid > 0.0) {
        maxBin = std::clamp(static_cast<int>(std::floor(fixed.fixedCentroid)), 1, totalBins - 2);
    } else {
        const double searchRad = fixed.useExactFitWin
            ? std::min(windowCh, std::max(3.5, (fixed.fixedFwhm > 0.2) ? (1.5 * fixed.fixedFwhm) : 5.0))
            : windowCh;
        const int sLow = std::max(1, static_cast<int>(std::floor(centerCh - searchRad)));
        const int sHigh = std::min(totalBins - 2, static_cast<int>(std::ceil(centerCh + searchRad)));
        maxBin = sLow;
        double maxVal = spectrum[sLow];
        for (int ch = sLow; ch <= sHigh; ++ch) {
            if (spectrum[ch] > maxVal) {
                maxVal = spectrum[ch];
                maxBin = ch;
            }
        }
    }
    double maxVal = (maxBin >= 0 && maxBin < totalBins) ? spectrum[maxBin] : 0.0;

    // Baseline estimation from endpoints or fixed baseline
    double baseVal = fixed.fixBaseline ? fixed.fixedBaseline : (spectrum[lowCh] + spectrum[highCh]) / 2.0;
    if (!fixed.fixBaseline && baseVal >= maxVal) {
        baseVal = std::min(spectrum[lowCh], spectrum[highCh]);
    }
    if (!fixed.fixBaseline && baseVal >= maxVal) {
        baseVal = std::max(0.0, maxVal * 0.1);
    }
    double netHeight = maxVal - baseVal;
    if (netHeight < minCounts && !fixed.fixCentroid) {
        return result; // Low counts
    }

    // 2. Estimate empirical FWHM via half-maximum walk from maxBin (GASP bin center = ch + 0.5)
    double halfMax = baseVal + 0.5 * (maxVal - baseVal);
    double leftHM = static_cast<double>(lowCh) + 0.5;
    for (int ch = maxBin; ch >= lowCh; --ch) {
        if (spectrum[ch] <= halfMax) {
            double y1 = spectrum[ch];
            double y2 = (ch + 1 <= maxBin) ? spectrum[ch + 1] : halfMax;
            double frac = (std::abs(y2 - y1) > 1e-6) ? (halfMax - y1) / (y2 - y1) : 0.0;
            leftHM = (static_cast<double>(ch) + 0.5) + frac;
            break;
        }
    }
    double rightHM = static_cast<double>(highCh) + 0.5;
    for (int ch = maxBin; ch <= highCh; ++ch) {
        if (spectrum[ch] <= halfMax) {
            double y1 = (ch - 1 >= maxBin) ? spectrum[ch - 1] : halfMax;
            double y2 = spectrum[ch];
            double frac = (std::abs(y1 - y2) > 1e-6) ? (halfMax - y2) / (y1 - y2) : 0.0;
            rightHM = (static_cast<double>(ch) + 0.5) - frac;
            break;
        }
    }
    double obsFwhm = rightHM - leftHM;
    double estFwhm = (obsFwhm >= 0.8 && obsFwhm <= windowCh * 1.5)
        ? obsFwhm
        : (fixed.fixFwhm ? fixed.fixedFwhm : std::clamp(windowCh * 0.25, 1.5, 6.0));

    // 3. Deterministic fit window around maxBin (Xtrackn TRSETREG fwfact = 2.5: po +- 2.5 * FWHM, or exact user window)
    const double estSigma = fixed.fixFwhm ? std::max(0.1, fixed.fixedFwhm / 2.35482) : std::max(0.2, estFwhm / 2.35482);
    const int halfWin = fixed.useExactFitWin
        ? std::clamp(static_cast<int>(std::round(windowCh)), 3, 200)
        : std::clamp(static_cast<int>(std::round(2.5 * 2.35482 * estSigma)),
                     5,
                     std::max(6, static_cast<int>(std::round(windowCh * 1.5))));
    const int fitMin = std::max(1, maxBin - halfWin);
    const int fitMax = std::min(totalBins - 2, maxBin + halfWin);
    const int nFitBins = fitMax - fitMin + 1;
    if (nFitBins < 5) return result;

    // Extract local SNIP continuum across [fitMin - margin, fitMax + margin]
    const int snipMargin = std::clamp(static_cast<int>(std::round(6.0 * estFwhm)), 25, 100);
    const int snipStart = std::max(0, fitMin - snipMargin);
    const int snipEnd = std::min(totalBins - 1, fitMax + snipMargin);
    std::vector<double> localSlice(spectrum.begin() + snipStart, spectrum.begin() + snipEnd + 1);
    const int snipIters = std::clamp(static_cast<int>(std::round(2.5 * estFwhm)), 12, 35);
    const std::vector<double> localSnip = computeSnipContinuum(localSlice, snipIters);

    auto snipAtBin = [&](int binIdx) -> double {
        const int idx = std::clamp(binIdx - snipStart, 0, static_cast<int>(localSnip.size()) - 1);
        return std::max(0.0, localSnip[idx]);
    };

    // Baseline estimation from SNIP continuum (cross-checked against minimum 3-bin edge average)
    if (!fixed.fixBaseline) {
        double leftSum = 0.0, rightSum = 0.0;
        int nLeft = 0, nRight = 0;
        for (int c = fitMin; c <= std::min(fitMax, fitMin + 2); ++c) {
            leftSum += spectrum[c];
            nLeft++;
        }
        for (int c = std::max(fitMin, fitMax - 2); c <= fitMax; ++c) {
            rightSum += spectrum[c];
            nRight++;
        }
        const double leftAvg = (nLeft > 0) ? (leftSum / nLeft) : spectrum[fitMin];
        const double rightAvg = (nRight > 0) ? (rightSum / nRight) : spectrum[fitMax];
        const double edgeMin = std::max(0.0, std::min(leftAvg, rightAvg));
        const double edgeMean = std::max(0.0, 0.5 * (leftAvg + rightAvg));
        const double snipCenter = snipAtBin(maxBin);
        baseVal = (snipCenter > 0.25) ? std::min(snipCenter, std::max(edgeMin, edgeMean)) : edgeMin;
        if (baseVal >= maxVal) {
            baseVal = std::max(0.0, maxVal * 0.1);
        }
    }

    // Use a lightweight ROOT TH1F with GASP [fitMin, fitMax + 1.0] bin edges so channel k has center k + 0.5
    TH1F hTemp("hAnchorTemp", "Anchor", nFitBins, static_cast<double>(fitMin), static_cast<double>(fitMax) + 1.0);
    hTemp.SetDirectory(nullptr);
    for (int i = 0; i < nFitBins; ++i) {
        const int ch = fitMin + i;
        hTemp.SetBinContent(i + 1, spectrum[ch]);
        hTemp.SetBinError(i + 1, std::max(1.0, std::sqrt(std::max(0.0, spectrum[ch]))));
    }

    const double estAmpl = std::max(1.0, maxVal - baseVal);
    const double estMean = fixed.fixCentroid ? fixed.fixedCentroid : (static_cast<double>(maxBin) + 0.5);

    // Fit a straight-line chord L(x) = meanSnip + slopeSnip*(x - xMid) to localSnip across [fitMin, fitMax]
    const double xMid = 0.5 * static_cast<double>(fitMin + fitMax) + 0.5;
    double sumS = 0.0, sumXS = 0.0, sumXX = 0.0;
    for (int ch = fitMin; ch <= fitMax; ++ch) {
        const double dx = (static_cast<double>(ch) + 0.5) - xMid;
        const double sVal = snipAtBin(ch);
        sumS += sVal;
        sumXS += dx * sVal;
        sumXX += dx * dx;
    }
    const double meanSnipChord = sumS / static_cast<double>(nFitBins);
    const double rawSlopeChord = (sumXX > 1e-9) ? (sumXS / sumXX) : 0.0;
    const double maxAbsSlope = 0.05 * std::max(0.25, meanSnipChord);
    const double slopeSnipChord = (meanSnipChord > 0.25)
        ? std::clamp(rawSlopeChord, -maxAbsSlope, maxAbsSlope)
        : 0.0;

    // Gaussian + Fixed-Slope Straight-Chord SNIP Background: [0]*exp(-0.5*((x-[1])/[2])^2) + [3] + slope*(x - [1])
    TF1 fGaus("fAnchorGaus",
              [slopeSnipChord](double *x, double *p) -> double {
                  const double dx = (x[0] - p[1]) / p[2];
                  const double gaus = p[0] * std::exp(-0.5 * dx * dx);
                  const double bkg = std::max(0.0, p[3] + slopeSnipChord * (x[0] - p[1]));
                  return gaus + bkg;
              },
              static_cast<double>(fitMin), static_cast<double>(fitMax) + 1.0, 4);

    const double ampLow = 0.0;
    const double ampHigh = std::max(10.0, maxVal * 4.0);
    fGaus.SetParameter(0, std::clamp(estAmpl, ampLow, ampHigh));
    fGaus.SetParLimits(0, ampLow, ampHigh);

    if (fixed.fixCentroid) {
        fGaus.FixParameter(1, fixed.fixedCentroid);
    } else {
        const double meanLow = static_cast<double>(fitMin);
        const double meanHigh = static_cast<double>(fitMax) + 1.0;
        fGaus.SetParameter(1, std::clamp(estMean, meanLow, meanHigh));
        fGaus.SetParLimits(1, meanLow, meanHigh);
    }

    if (fixed.fixFwhm) {
        fGaus.FixParameter(2, std::max(0.1, fixed.fixedFwhm / 2.35482));
    } else {
        const double sigLow = 0.2;
        const double sigHigh = 25.0;
        fGaus.SetParameter(2, std::clamp(estSigma, sigLow, sigHigh));
        fGaus.SetParLimits(2, sigLow, sigHigh);
    }

    if (fixed.fixBaseline) {
        fGaus.FixParameter(3, fixed.fixedBaseline);
    } else {
        // Tightly constrain background around the SNIP continuum so the fit cannot push background into the peak
        const double bkgLow = std::max(0.0, 0.85 * baseVal - 0.5);
        const double maxHeadroom = std::max(1.0, 0.15 * baseVal + 0.5 * std::sqrt(std::max(1.0, baseVal)));
        const double bkgHigh = baseVal + std::min(maxHeadroom, std::max(0.5, 0.15 * estAmpl));
        fGaus.SetParameter(3, std::clamp(baseVal, bkgLow, bkgHigh));
        fGaus.SetParLimits(3, bkgLow, bkgHigh);
    }

    // Fit with Poisson Likelihood + Bin Integration ("Q0NSIL"), fallback to standard Chi2 ("Q0NS")
    TFitResultPtr fitRes = hTemp.Fit(&fGaus, "Q0NSIL", "", static_cast<double>(fitMin), static_cast<double>(fitMax) + 1.0);
    if (!fitRes.Get() || (!fitRes->IsValid() && fitRes->Status() != 0 && !(fixed.fixCentroid && (fitRes->Status() % 1000 == 0 || fitRes->Status() == 4)))) {
        fitRes = hTemp.Fit(&fGaus, "Q0NS", "", static_cast<double>(fitMin), static_cast<double>(fitMax) + 1.0);
    }
    const bool fitValid = fitRes.Get() && (fitRes->IsValid() || fitRes->Status() == 0 || (fixed.fixCentroid && (fitRes->Status() % 1000 == 0 || fitRes->Status() == 4)));
    if (fitValid) {
        const double fittedMean = fixed.fixCentroid ? fixed.fixedCentroid : fGaus.GetParameter(1);
        const double fittedSigma = std::abs(fGaus.GetParameter(2));
        const double fittedAmpl = fGaus.GetParameter(0);
        const double fwhm = 2.35482 * fittedSigma;
        const double area = fittedAmpl * fittedSigma * std::sqrt(2.0 * M_PI);

        // Explicit Pearson chi2 across fit bins
        double calcChi2 = 0.0;
        for (int i = 0; i < nFitBins; ++i) {
            const double data = hTemp.GetBinContent(i + 1);
            const double expected = fGaus.Eval(hTemp.GetBinCenter(i + 1));
            const double var = std::max(1.0, data);
            calcChi2 += (data - expected) * (data - expected) / var;
        }
        const int nFree = fGaus.GetNumberFreeParameters();
        const int ndf = std::max(1, nFitBins - nFree);
        const double chi2 = calcChi2 / ndf;

        double fittedMeanErr = 0.05;
        if (fixed.fixCentroid) {
            const double statErr = (area > 1.0) ? (fittedSigma / std::sqrt(area)) : 0.05;
            const double bkgVal = std::max(0.0, fGaus.GetParameter(3));
            const double bkgFactor = (area > 1.0 && bkgVal > 0.0)
                ? std::sqrt(1.0 + (2.0 * bkgVal * fwhm) / area)
                : 1.0;
            fittedMeanErr = std::clamp(statErr * bkgFactor, 0.03, 0.35);
        } else {
            fittedMeanErr = std::max(0.01, fGaus.GetParError(1));
        }

        if (fittedMean >= static_cast<double>(lowCh) && fittedMean <= static_cast<double>(highCh) + 1.0
            && fwhm >= 0.3 && fwhm <= 50.0 && fittedAmpl > 0.0) {
            result.centroidCh = fittedMean;
            result.centroidErrCh = fittedMeanErr;
            result.fwhmCh = fwhm;
            result.amplitude = fittedAmpl;
            result.background = fGaus.GetParameter(3);
            result.area = area;
            result.chi2 = chi2;
            result.fitWinCh = static_cast<double>(halfWin);
            result.isValid = true;
            return result;
        }
    }

    // Fallback: If parameters were manually fixed, return exact user values
    if (fixed.fixCentroid || fixed.fixFwhm || fixed.fixBaseline) {
        result.centroidCh = fixed.fixCentroid ? fixed.fixedCentroid : estMean;
        const double sig = (fixed.fixFwhm ? fixed.fixedFwhm : 3.5) / 2.35482;
        const double estArea = std::max(10.0, estAmpl * sig * std::sqrt(2.0 * M_PI));
        result.centroidErrCh = std::clamp(sig / std::sqrt(estArea), 0.03, 0.35);
        result.fwhmCh = fixed.fixFwhm ? fixed.fixedFwhm : 3.5;
        result.amplitude = estAmpl;
        result.background = fixed.fixBaseline ? fixed.fixedBaseline : baseVal;
        result.area = estArea;
        result.chi2 = 1.0;
        result.fitWinCh = static_cast<double>(halfWin);
        result.isValid = true;
        return result;
    }

    // Fallback: Background-subtracted centroid & moment (GASP bin center = ch + 0.5)
    double sumW = 0.0, sumWX = 0.0, sumWXX = 0.0;
    const int momMin = std::max(1, maxBin - 6);
    const int momMax = std::min(totalBins - 1, maxBin + 6);
    const double momBase = (spectrum[momMin] + spectrum[momMax]) / 2.0;

    for (int ch = momMin; ch <= momMax; ++ch) {
        const double w = std::max(0.0, spectrum[ch] - momBase);
        const double chCenter = static_cast<double>(ch) + 0.5;
        sumW += w;
        sumWX += w * chCenter;
        sumWXX += w * chCenter * chCenter;
    }

    if (sumW > minCounts) {
        const double centroid = sumWX / sumW;
        const double variance = std::max(0.5, (sumWXX / sumW) - (centroid * centroid));
        const double sigma = std::sqrt(variance);
        result.centroidCh = centroid;
        result.centroidErrCh = std::max(0.02, sigma / std::sqrt(sumW));
        result.fwhmCh = 2.35482 * sigma;
        result.amplitude = std::max(1.0, maxVal - momBase);
        result.background = momBase;
        result.area = sumW;
        result.chi2 = 1.0;
        result.fitWinCh = static_cast<double>(halfWin);
        result.isValid = (centroid >= static_cast<double>(lowCh) && centroid <= static_cast<double>(highCh) + 1.0);
    }

    return result;
}

//==============================================================================
// Coupled Multi-Peak Spectrum Fitter (Xtrackn boundfit + CERN ROOT Minimizer)
//==============================================================================
namespace {

struct BoundPeakSeed {
    int anchorIdx{0};
    double energy{0.0};
    double energyErr{0.0};
    double uNorm{0.0};        // E / nominalDispersion (nominal channel units)
    double seedCh{0.0};       // Local empirical center from initial window scan
    double seedErrCh{0.1};    // Statistical error on local center
    double seedSigmaCh{1.5};  // Initial sigma estimate in channels
    double seedAmpl{10.0};    // Initial peak height above background
    double seedBkg{1.0};      // Initial flat background estimate
    double windowCh{10.0};    // Search window half-width
    double expFwhmCh{4.0};    // Expected FWHM in channels
    int regionIdx{0};         // Assigned merged fit region index
};

struct BoundFitRegion {
    int c1{1};                // Left bin index (inclusive)
    int c2{2};                // Right bin index (inclusive)
    double initBkg{0.0};      // Initial mean background for this region
    double meanSnip{0.0};     // Mean SNIP continuum across [c1, c2]
    double xCenterReg{0.0};   // Geometric center coordinate 0.5*(c1 + c2) + 0.5
    double slopeSnip{0.0};    // Fixed linear chord slope (counts/ch) fitted to SNIP across [c1, c2]
    std::vector<int> peakIndices; // Indices into activePeaks vector
};

// Xtrackn 3-point binomial smoothing kernel: [0.25, 0.50, 0.25] applied m passes (trackn.F L5405-L5426)
void smoothSpectrumBinomial(std::vector<double> &spec, int nPasses)
{
    const int n = static_cast<int>(spec.size());
    if (n <= 4 || nPasses <= 0) return;
    std::vector<double> tmp(n, 0.0);
    for (int pass = 0; pass < nPasses; ++pass) {
        tmp[0] = 0.75 * spec[0] + 0.25 * spec[1];
        for (int i = 1; i < n - 1; ++i) {
            tmp[i] = 0.25 * spec[i - 1] + 0.50 * spec[i] + 0.25 * spec[i + 1];
        }
        tmp[n - 1] = 0.25 * spec[n - 2] + 0.75 * spec[n - 1];
        spec.swap(tmp);
    }
}

// Solve 2x2 or 3x3 weighted polynomial y(x) = c0 + c1*x (+ c2*x^2)
bool solveSmallPoly(const std::vector<double> &x,
                    const std::vector<double> &y,
                    const std::vector<double> &w,
                    int nPar,
                    std::vector<double> &outCoeffs)
{
    const size_t N = x.size();
    if (N < static_cast<size_t>(nPar) || nPar < 2 || nPar > 3) return false;
    outCoeffs.assign(nPar, 0.0);

    if (nPar == 2) {
        double sw = 0.0, swx = 0.0, swy = 0.0, swxx = 0.0, swxy = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double wi = w[i];
            sw   += wi;
            swx  += wi * x[i];
            swy  += wi * y[i];
            swxx += wi * x[i] * x[i];
            swxy += wi * x[i] * y[i];
        }
        const double det = sw * swxx - swx * swx;
        if (std::abs(det) < 1e-12) return false;
        outCoeffs[0] = (swxx * swy - swx * swxy) / det;
        outCoeffs[1] = (sw * swxy - swx * swy) / det;
        return true;
    }

    double m[3][3] = {{0.0}};
    double b[3] = {0.0};
    for (size_t i = 0; i < N; ++i) {
        const double wi = w[i];
        const double xi = x[i];
        const double x2 = xi * xi;
        const double x3 = x2 * xi;
        const double x4 = x2 * x2;
        m[0][0] += wi;      m[0][1] += wi * xi;  m[0][2] += wi * x2;
        m[1][0] += wi * xi; m[1][1] += wi * x2;  m[1][2] += wi * x3;
        m[2][0] += wi * x2; m[2][1] += wi * x3;  m[2][2] += wi * x4;
        b[0] += wi * y[i];
        b[1] += wi * xi * y[i];
        b[2] += wi * x2 * y[i];
    }
    const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::abs(det) < 1e-15) {
        return solveSmallPoly(x, y, w, 2, outCoeffs);
    }
    const double invDet = 1.0 / det;
    double inv[3][3];
    inv[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
    inv[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

    outCoeffs[0] = inv[0][0] * b[0] + inv[0][1] * b[1] + inv[0][2] * b[2];
    outCoeffs[1] = inv[1][0] * b[0] + inv[1][1] * b[1] + inv[1][2] * b[2];
    outCoeffs[2] = inv[2][0] * b[0] + inv[2][1] * b[1] + inv[2][2] * b[2];
    return true;
}

// Build and merge fit regions around bound peak positions po(u_i) +- 1.5 * FWHM(u_i) (Xtrackn TRSETREG + TRSET)
void buildBoundRegions(const std::vector<double> &spectrum,
                       const std::vector<double> &snipBg,
                       std::vector<BoundPeakSeed> &activePeaks,
                       const std::vector<double> &pCoeffs,
                       double w0, double w1,
                       std::vector<BoundFitRegion> &outRegions)
{
    outRegions.clear();
    const int totalBins = static_cast<int>(spectrum.size());
    const int nPeaks = static_cast<int>(activePeaks.size());
    if (nPeaks == 0 || totalBins <= 10) return;

    struct PeakWin {
        int peakIdx;
        double po;
        int c1;
        int c2;
    };
    std::vector<PeakWin> wins;
    wins.reserve(nPeaks);

    for (int i = 0; i < nPeaks; ++i) {
        const double u = activePeaks[i].uNorm;
        double po = pCoeffs[0] + pCoeffs[1] * u;
        if (pCoeffs.size() >= 3) po += pCoeffs[2] * u * u;

        const double s2 = std::clamp(std::abs(w0 + w1 * (u / 1000.0)), 0.09, 250.0);
        const double fwhm = 2.35482 * std::sqrt(s2);
        const int maxHalfWin = std::max(6, static_cast<int>(std::round(activePeaks[i].windowCh * 1.5)));
        const int halfWin = std::clamp(static_cast<int>(std::round(2.5 * fwhm)), 5, maxHalfWin);

        const int centerBin = std::clamp(static_cast<int>(std::floor(po)), 1, totalBins - 2);
        const int c1 = std::max(1, centerBin - halfWin);
        const int c2 = std::min(totalBins - 2, centerBin + halfWin);
        wins.push_back({i, po, c1, c2});
    }

    std::sort(wins.begin(), wins.end(), [](const PeakWin &a, const PeakWin &b) {
        return a.po < b.po;
    });

    // Merge overlapping windows so adjacent peaks share a single continuum background region (Xtrackn TRSETREG L5182-L5217)
    for (const auto &w : wins) {
        if (!outRegions.empty() && w.c1 <= outRegions.back().c2) {
            outRegions.back().c2 = std::max(outRegions.back().c2, w.c2);
            outRegions.back().peakIndices.push_back(w.peakIdx);
            activePeaks[w.peakIdx].regionIdx = static_cast<int>(outRegions.size()) - 1;
        } else {
            BoundFitRegion reg;
            reg.c1 = w.c1;
            reg.c2 = w.c2;
            reg.peakIndices.push_back(w.peakIdx);
            activePeaks[w.peakIdx].regionIdx = static_cast<int>(outRegions.size());
            outRegions.push_back(reg);
        }
    }

    // Estimate initial mean background B0_r and mean SNIP continuum across [c1, c2]
    const bool hasSnip = (snipBg.size() == spectrum.size());
    for (auto &reg : outRegions) {
        double leftSum = 0.0, rightSum = 0.0;
        int nLeft = 0, nRight = 0;
        for (int c = reg.c1; c <= std::min(reg.c2, reg.c1 + 2); ++c) {
            leftSum += spectrum[c];
            nLeft++;
        }
        for (int c = std::max(reg.c1, reg.c2 - 2); c <= reg.c2; ++c) {
            rightSum += spectrum[c];
            nRight++;
        }
        const double leftAvg = (nLeft > 0) ? (leftSum / nLeft) : spectrum[reg.c1];
        const double rightAvg = (nRight > 0) ? (rightSum / nRight) : spectrum[reg.c2];
        const double edgeMinBkg = std::max(0.0, std::min(leftAvg, rightAvg));
        const double edgeMeanBkg = std::max(0.0, 0.5 * (leftAvg + rightAvg));

        reg.xCenterReg = 0.5 * static_cast<double>(reg.c1 + reg.c2) + 0.5;
        double snipSum = 0.0, snipXSum = 0.0, snipXXSum = 0.0;
        int nSnip = 0;
        if (hasSnip) {
            for (int c = reg.c1; c <= reg.c2; ++c) {
                const double sVal = std::max(0.0, snipBg[c]);
                const double dx = (static_cast<double>(c) + 0.5) - reg.xCenterReg;
                snipSum += sVal;
                snipXSum += dx * sVal;
                snipXXSum += dx * dx;
                nSnip++;
            }
        }
        reg.meanSnip = (nSnip > 0) ? (snipSum / nSnip) : 0.0;
        const double rawSlope = (snipXXSum > 1e-9) ? (snipXSum / snipXXSum) : 0.0;
        const double maxAbsSlope = 0.05 * std::max(0.25, reg.meanSnip);
        reg.slopeSnip = (reg.meanSnip > 0.25)
            ? std::clamp(rawSlope, -maxAbsSlope, maxAbsSlope)
            : 0.0;
        reg.initBkg = (reg.meanSnip > 0.25) ? std::min(reg.meanSnip, std::max(edgeMinBkg, edgeMeanBkg)) : edgeMinBkg;
    }
}

// Multi-region coupled objective function (Baker-Cousins Poisson Likelihood Deviance with SNIP Continuum)
class CoupledBoundFCN : public ROOT::Math::IBaseFunctionMultiDim {
public:
    CoupledBoundFCN(const std::vector<double> &spectrum,
                    const std::vector<double> &snipBg,
                    const std::vector<BoundPeakSeed> &peaks,
                    const std::vector<BoundFitRegion> &regions,
                    int nEnergyPars,
                    double nominalDispersion = 1.0)
        : m_spectrum(spectrum),
          m_snipBg(snipBg),
          m_peaks(peaks),
          m_regions(regions),
          m_nE(nEnergyPars),
          m_disp(nominalDispersion > 0.0 ? nominalDispersion : 1.0)
    {
        m_nDim = m_nE + 2 + static_cast<int>(m_regions.size()) + static_cast<int>(m_peaks.size());

        // Precompute balanced region weights w_r = statVar / (statVar + energyVar)
        // so huge low-energy peaks (200-279 keV) cannot overpower high-energy anchor peaks (948-1054 keV)
        m_regWeights.resize(m_regions.size(), 1.0);
        for (size_t r = 0; r < m_regions.size(); ++r) {
            double estArea = 0.0;
            double minSigE = 1.0;
            double sigCh = 1.5;
            for (int pIdx : m_regions[r].peakIndices) {
                if (pIdx >= 0 && pIdx < static_cast<int>(m_peaks.size())) {
                    estArea += std::max(5.0, m_peaks[pIdx].seedAmpl * m_peaks[pIdx].seedSigmaCh * std::sqrt(2.0 * M_PI));
                    sigCh = m_peaks[pIdx].seedSigmaCh;
                    const double sigECh = m_peaks[pIdx].energyErr / m_disp;
                    minSigE = std::min(minSigE, sigECh);
                }
            }
            const double statVar = (sigCh * sigCh) / std::max(5.0, estArea);
            const double energyVar = minSigE * minSigE;
            m_regWeights[r] = statVar / (statVar + energyVar);
        }
    }

    unsigned int NDim() const override {
        return static_cast<unsigned int>(m_nDim);
    }

    ROOT::Math::IBaseFunctionMultiDim* Clone() const override {
        return new CoupledBoundFCN(m_spectrum, m_snipBg, m_peaks, m_regions, m_nE, m_disp);
    }

    double DoEval(const double *par) const override {
        // Parameter layout:
        // [0 .. m_nE-1]                    : p_0, p_1 (, p_2) inverse calibration polynomial po(u)
        // [m_nE, m_nE+1]                   : w_0, w_1 width-squared polynomial s^2(u) = w_0 + w_1*(u/1000)
        // [m_nE+2 .. m_nE+2+nReg-1]        : B0_r mean continuum background per region
        // [m_nE+2+nReg .. m_nDim-1]        : A_i amplitude per peak
        const int nReg = static_cast<int>(m_regions.size());
        const int nPeaks = static_cast<int>(m_peaks.size());
        const int bkgOffset = m_nE + 2;
        const int ampOffset = bkgOffset + nReg;
        const bool hasSnip = (m_snipBg.size() == m_spectrum.size());

        const double w0 = par[m_nE];
        const double w1 = par[m_nE + 1];

        // Precompute bound centroid po_i and variance s2_i for each peak (Xtrackn TRFUN L5340-L5348)
        double po[32];
        double inv2s2[32];
        for (int i = 0; i < nPeaks && i < 32; ++i) {
            const double u = m_peaks[i].uNorm;
            double pos = par[0] + par[1] * u;
            if (m_nE >= 3) pos += par[2] * u * u;
            po[i] = pos;

            const double rawS2 = w0 + w1 * (u / 1000.0);
            const double s2 = std::clamp(std::abs(rawS2), 0.06, 400.0);
            inv2s2[i] = 0.5 / s2;
        }

        // Baker-Cousins Poisson likelihood deviance across all regions and bins,
        // weighted by m_regWeights so high-amplitude peak clusters cannot overwhelm weaker anchors
        double deviance = 0.0;
        for (int r = 0; r < nReg; ++r) {
            const auto &reg = m_regions[r];
            const double wReg = (r < static_cast<int>(m_regWeights.size())) ? m_regWeights[r] : 1.0;
            const double bkgMean = std::max(0.0, par[bkgOffset + r]);
            const bool useSnipChord = hasSnip && (reg.meanSnip > 0.25);

            double regDev = 0.0;
            for (int ch = reg.c1; ch <= reg.c2; ++ch) {
                const double xCenter = static_cast<double>(ch) + 0.5;
                double yModel = useSnipChord
                    ? std::max(0.0, bkgMean + reg.slopeSnip * (xCenter - reg.xCenterReg))
                    : bkgMean;
                for (int pIdx : reg.peakIndices) {
                    if (pIdx < 32) {
                        const double dx = xCenter - po[pIdx];
                        const double arg = dx * dx * inv2s2[pIdx];
                        if (arg < 40.0) {
                            yModel += std::max(0.0, par[ampOffset + pIdx]) * std::exp(-arg);
                        }
                    }
                }
                yModel = std::max(1e-6, yModel);
                const double nObs = std::max(0.0, m_spectrum[ch]);
                if (nObs > 1e-9) {
                    regDev += 2.0 * (yModel - nObs + nObs * std::log(nObs / yModel));
                } else {
                    regDev += 2.0 * yModel;
                }
            }
            deviance += wReg * regDev;
        }
        return deviance;
    }

private:
    const std::vector<double> &m_spectrum;
    const std::vector<double> &m_snipBg;
    const std::vector<BoundPeakSeed> &m_peaks;
    const std::vector<BoundFitRegion> &m_regions;
    int m_nE{2};
    double m_disp{1.0};
    int m_nDim{0};
    std::vector<double> m_regWeights;
};

// Objective function for whole-spectrum cross-correlation gain/shift pre-alignment (Xtrackn gainfit L4710-L4834)
class GainFitCrossCorrFCN : public ROOT::Math::IBaseFunctionMultiDim {
public:
    GainFitCrossCorrFCN(const std::vector<double> &prevSpec,
                        const std::vector<double> &currSpec,
                        int minBin, int maxBin,
                        double scaleNorm)
        : m_prev(prevSpec),
          m_curr(currSpec),
          m_minBin(minBin),
          m_maxBin(maxBin),
          m_scaleNorm(scaleNorm)
    {}

    unsigned int NDim() const override { return 2; }

    ROOT::Math::IBaseFunctionMultiDim* Clone() const override {
        return new GainFitCrossCorrFCN(m_prev, m_curr, m_minBin, m_maxBin, m_scaleNorm);
    }

    double DoEval(const double *par) const override {
        // par[0] = sf0 (shift in compressed bins)
        // par[1] = sf1 (scale factor ~ 1.0)
        const double sf0 = par[0];
        const double sf1 = par[1];
        const int nBins = static_cast<int>(m_curr.size());
        double sumSqDiff = 0.0;

        for (int j = m_minBin; j <= m_maxBin; ++j) {
            const double yPos = sf0 + sf1 * static_cast<double>(j);
            double currInterp = 0.0;
            if (yPos >= 1.0 && yPos < static_cast<double>(nBins - 2)) {
                const int idx = static_cast<int>(std::floor(yPos));
                const double frac = yPos - static_cast<double>(idx);
                currInterp = (1.0 - frac) * m_curr[idx] + frac * m_curr[idx + 1];
            }
            const double diff = m_prev[j] - m_scaleNorm * currInterp;
            sumSqDiff += diff * diff;
        }
        return sumSqDiff;
    }

private:
    const std::vector<double> &m_prev;
    const std::vector<double> &m_curr;
    int m_minBin{1};
    int m_maxBin{1022};
    double m_scaleNorm{1.0};
};

} // anonymous namespace

//==============================================================================
// computeSnipContinuum: Smooth 1D SNIP Compton continuum extraction
//==============================================================================
std::vector<double> RunByRunEngine::computeSnipContinuum(const std::vector<double> &spectrum,
                                                         int iterations)
{
    const int n = static_cast<int>(spectrum.size());
    if (n <= 4) return spectrum;

    // 1. Pre-smooth with 3 passes of 3-point binomial filter so single-run zero/low-count Poisson dips
    //    do not pull the peak-clipping envelope artificially downward.
    std::vector<double> bg = spectrum;
    for (int i = 0; i < n; ++i) {
        if (bg[i] < 0.0) bg[i] = 0.0;
    }
    smoothSpectrumBinomial(bg, 3);

    // 2. Iterative SNIP peak-clipping filter with decreasing window (p = m down to 1, Morhac NIM A 401):
    //    Pass m first cuts a flat bridge across the base of each peak, and passes m-1 .. 1 bisect the corners,
    //    eliminating triangular tent kinks under peak centers.
    const int m = std::clamp(iterations, 4, 60);
    std::vector<double> nextBg = bg;
    for (int p = m; p >= 1; --p) {
        for (int i = p; i < n - p; ++i) {
            const double avg = 0.5 * (bg[i - p] + bg[i + p]);
            nextBg[i] = (avg < bg[i]) ? avg : bg[i];
        }
        bg.swap(nextBg);
    }

    // 3. Light post-smoothing so the continuum derivative is continuous across peak windows
    smoothSpectrumBinomial(bg, 2);
    return bg;
}

double RunByRunEngine::evalSnipAt(const std::vector<double> &snipBg, double channelX)
{
    if (snipBg.empty()) return 0.0;
    const int n = static_cast<int>(snipBg.size());
    const double u = channelX - 0.5; // Continuous 0-based bin index (bin k has center k + 0.5)
    if (u <= 0.0) return std::max(0.0, snipBg.front());
    if (u >= static_cast<double>(n - 1)) return std::max(0.0, snipBg.back());
    const int k = static_cast<int>(std::floor(u));
    const double frac = u - static_cast<double>(k);
    return std::max(0.0, (1.0 - frac) * snipBg[k] + frac * snipBg[k + 1]);
}

//==============================================================================
// preAlignGainFit: Whole-spectrum cross-correlation pre-alignment (Xtrackn gainfit)
//==============================================================================
bool RunByRunEngine::preAlignGainFit(const std::vector<double> &prevPeakSpectrum,
                                     const std::vector<double> &currPeakSpectrum,
                                     const std::vector<double> &prevCentroids,
                                     double maxAllowedDriftCh,
                                     std::vector<double> &outExpectedCentroids)
{
    outExpectedCentroids = prevCentroids;
    const int totalBins = static_cast<int>(std::min(prevPeakSpectrum.size(), currPeakSpectrum.size()));
    if (totalBins <= 64 || prevCentroids.empty()) return false;

    // Determine active channel span covering all anchor peaks (+ margin)
    double minAnchor = prevCentroids.front();
    double maxAnchor = prevCentroids.front();
    for (double c : prevCentroids) {
        if (c > 0.0) {
            minAnchor = std::min(minAnchor, c);
            maxAnchor = std::max(maxAnchor, c);
        }
    }
    if (maxAnchor <= 10.0) return false;

    // Choose power-of-2 compression factor fackap (1, 2, 4, or 8 ch/bin) so active range fits in ~1024 compressed bins
    // (mirrors Xtrackn gainfit L4715-L4730: nkap = len/1024)
    const int activeMaxCh = std::clamp(static_cast<int>(std::ceil(maxAnchor * 1.25 + 150.0)), 512, totalBins);
    int fackap = 1;
    while ((activeMaxCh / fackap) > 1024 && fackap < 16) {
        fackap *= 2;
    }
    const int nComp = activeMaxCh / fackap;
    if (nComp < 64) return false;

    std::vector<double> compPrev(nComp, 0.0);
    std::vector<double> compCurr(nComp, 0.0);
    for (int i = 0; i < nComp; ++i) {
        double sumP = 0.0, sumC = 0.0;
        const int base = i * fackap;
        for (int k = 0; k < fackap && (base + k) < totalBins; ++k) {
            sumP += std::max(0.0, prevPeakSpectrum[base + k]);
            sumC += std::max(0.0, currPeakSpectrum[base + k]);
        }
        compPrev[i] = sumP;
        compCurr[i] = sumC;
    }

    // Smooth compressed peak spectra with 2 passes of 3-point binomial filter (Xtrackn gainfit L4736-L4737: call smooth(..., 2))
    smoothSpectrumBinomial(compPrev, 2);
    smoothSpectrumBinomial(compCurr, 2);

    const int minCompBin = std::clamp(static_cast<int>(std::floor(std::max(20.0, minAnchor - 80.0) / fackap)),
                                      4, nComp - 16);
    const int maxCompBin = std::clamp(static_cast<int>(std::ceil(std::min(static_cast<double>(activeMaxCh - 20), maxAnchor + 80.0) / fackap)),
                                      minCompBin + 16, nComp - 5);

    double totPrev = 0.0, totCurr = 0.0;
    for (int j = minCompBin; j <= maxCompBin; ++j) {
        totPrev += compPrev[j];
        totCurr += compCurr[j];
    }
    if (totPrev <= 20.0 || totCurr <= 20.0) return false;
    const double scaleNorm = totPrev / totCurr;

    GainFitCrossCorrFCN fcn(compPrev, compCurr, minCompBin, maxCompBin, scaleNorm);
    const double identityPar[2] = {0.0, 1.0};
    const double identityCost = fcn.DoEval(identityPar);
    if (identityCost <= 1e-9) return false;

    // 1. Coarse 1D shift scan across [-maxDriftComp, +maxDriftComp] to avoid local minima on sudden gain jumps
    const double maxDriftComp = std::max(2.0, maxAllowedDriftCh / static_cast<double>(fackap));
    double bestSf0 = 0.0;
    double bestScanCost = identityCost;
    const int scanSteps = std::clamp(static_cast<int>(std::ceil(2.0 * maxDriftComp)), 4, 80);
    for (int s = -scanSteps; s <= scanSteps; ++s) {
        const double candSf0 = 0.5 * static_cast<double>(s);
        if (std::abs(candSf0) > maxDriftComp) continue;
        const double candPar[2] = {candSf0, 1.0};
        const double cCost = fcn.DoEval(candPar);
        if (cCost < bestScanCost) {
            bestScanCost = cCost;
            bestSf0 = candSf0;
        }
    }

    // 2. Refine (sf0, sf1) simultaneously using CERN ROOT Minuit2
    std::unique_ptr<ROOT::Math::Minimizer> minimizer(
        ROOT::Math::Factory::CreateMinimizer("Minuit2", "Migrad"));
    if (!minimizer) {
        minimizer.reset(ROOT::Math::Factory::CreateMinimizer("Minuit", "Migrad"));
    }

    double finalSf0 = bestSf0;
    double finalSf1 = 1.0;
    double finalCost = bestScanCost;

    if (minimizer) {
        minimizer->SetMaxFunctionCalls(600);
        minimizer->SetMaxIterations(300);
        minimizer->SetTolerance(0.01);
        minimizer->SetPrintLevel(0);
        minimizer->SetFunction(fcn);
        minimizer->SetLimitedVariable(0, "sf0", bestSf0, 0.1, -maxDriftComp, maxDriftComp);
        minimizer->SetLimitedVariable(1, "sf1", 1.0, 5e-4, 0.985, 1.015);
        minimizer->Minimize();

        const double *xs = minimizer->X();
        if (xs) {
            const double minCost = fcn.DoEval(xs);
            if (minCost < finalCost) {
                finalSf0 = xs[0];
                finalSf1 = xs[1];
                finalCost = minCost;
            }
        }
    }

    // Only apply pre-alignment if it reduces the spectrum cross-correlation residual
    if (finalCost >= identityCost * 0.995) {
        return false;
    }

    // Transform expected peak channels (Xtrackn gainfit L4827: yfit(ii) = sf0*fackap + yfit(ii)*sf1)
    const double shiftFullCh = finalSf0 * static_cast<double>(fackap);
    for (size_t i = 0; i < prevCentroids.size(); ++i) {
        const double shifted = shiftFullCh + finalSf1 * prevCentroids[i];
        if (std::abs(shifted - prevCentroids[i]) <= maxAllowedDriftCh) {
            outExpectedCentroids[i] = shifted;
        }
    }
    return true;
}

bool RunByRunEngine::fitSpectrumBound(const std::vector<double> &spectrum,
                                      const std::vector<AnchorPeakDef> &anchors,
                                      const std::vector<double> &initialExpectedChannels,
                                      const std::vector<double> &baseCentroids,
                                      bool useQuadratic,
                                      double nominalDispersion,
                                      double maxAllowedDriftCh,
                                      double maxAllowedChi2NDF,
                                      RunCalibResult &outResult,
                                      const std::vector<double> *precomputedSnip)
{
    const int totalBins = static_cast<int>(spectrum.size());
    const size_t nAnchors = anchors.size();
    const double disp = (nominalDispersion > 0.0) ? nominalDispersion : 1.0;
    outResult.nominalGain = disp;

    if (totalBins <= 10 || nAnchors == 0) {
        outResult.status = RunCalibStatus::FileError;
        outResult.failureReason = "Empty spectrum or no anchor peaks configured.";
        return false;
    }

    // 0. Extract smooth SNIP Compton continuum shape across the spectrum (or reuse precomputedSnip)
    double sumExpFwhm = 0.0;
    for (const auto &def : anchors) {
        sumExpFwhm += std::max(1.5, def.expectedFwhmCh);
    }
    const double avgExpFwhm = sumExpFwhm / static_cast<double>(nAnchors);
    const int snipIters = std::clamp(static_cast<int>(std::round(2.5 * avgExpFwhm)), 12, 35);
    std::vector<double> localSnip;
    if (!precomputedSnip || precomputedSnip->size() != spectrum.size()) {
        localSnip = computeSnipContinuum(spectrum, snipIters);
    }
    const std::vector<double> &snipBg = (precomputedSnip && precomputedSnip->size() == spectrum.size())
                                        ? *precomputedSnip : localSnip;

    // 1. Seed all configured anchor peaks (Xtrackn boundfit includes all ntrpeak peaks)
    std::vector<FittedAnchor> allAnchors(nAnchors);
    std::vector<BoundPeakSeed> activePeaks;
    activePeaks.reserve(nAnchors);

    for (size_t aIdx = 0; aIdx < nAnchors; ++aIdx) {
        const auto &def = anchors[aIdx];
        const double baseCh = (aIdx < baseCentroids.size()) ? baseCentroids[aIdx] : (def.physicalEnergy / disp);
        const double expCh = (aIdx < initialExpectedChannels.size()) ? initialExpectedChannels[aIdx] : baseCh;
        const double winCh = std::max(2.0, def.searchWindowCh);
        const double expFwhm = std::max(0.8, def.expectedFwhmCh);
        const double minCounts = std::max(1.0, def.minCounts);

        auto scanWindow = [&](double searchCenter, double scanHalfWin, double &outBkg, double &outNet, double &outMomCh, double &outSigma) -> bool {
            const int lowCh = std::max(1, static_cast<int>(std::floor(searchCenter - scanHalfWin)));
            const int highCh = std::min(totalBins - 2, static_cast<int>(std::ceil(searchCenter + scanHalfWin)));
            if (lowCh >= highCh) return false;

            // Search for highest continuum-subtracted peak bin inside [lowCh, highCh]
            int maxBin = lowCh;
            double maxNetVal = spectrum[lowCh] - snipBg[lowCh];
            for (int ch = lowCh + 1; ch <= highCh; ++ch) {
                const double netCandidate = spectrum[ch] - snipBg[ch];
                if (netCandidate > maxNetVal) {
                    maxNetVal = netCandidate;
                    maxBin = ch;
                }
            }
            const double maxVal = spectrum[maxBin];
            double bkg = std::max(0.0, snipBg[maxBin]);
            if (bkg >= maxVal) bkg = std::max(0.0, maxVal * 0.1);

            const double net = std::max(0.0, maxVal - bkg);

            // Local continuum-subtracted moment center around maxBin (+- FWHM), in GASP bin-center coordinates (c + 0.5)
            const int hw = std::clamp(static_cast<int>(std::round(2.5 * expFwhm)), 5, std::max(6, static_cast<int>(std::round(scanHalfWin * 1.5))));
            const int c1 = std::max(1, maxBin - hw);
            const int c2 = std::min(totalBins - 2, maxBin + hw);
            const int momHalf = std::clamp(static_cast<int>(std::round(0.85 * expFwhm)), 2, 8);
            const int m1 = std::max(1, maxBin - momHalf);
            const int m2 = std::min(totalBins - 2, maxBin + momHalf);
            double sw = 0.0, swx = 0.0;
            for (int c = m1; c <= m2; ++c) {
                const double w = std::max(0.0, spectrum[c] - snipBg[c]);
                sw += w;
                swx += w * (static_cast<double>(c) + 0.5);
            }
            const double momCh = (sw > 0.0) ? (swx / sw) : (static_cast<double>(maxBin) + 0.5);

            // Half-maximum width estimate
            const double halfMax = bkg + 0.5 * net;
            double lHM = static_cast<double>(c1) + 0.5, rHM = static_cast<double>(c2) + 0.5;
            for (int c = maxBin; c >= c1; --c) {
                if (spectrum[c] <= halfMax) {
                    const double y1 = spectrum[c], y2 = (c + 1 <= maxBin) ? spectrum[c + 1] : halfMax;
                    lHM = (static_cast<double>(c) + 0.5) + ((std::abs(y2 - y1) > 1e-6) ? (halfMax - y1) / (y2 - y1) : 0.0);
                    break;
                }
            }
            for (int c = maxBin; c <= c2; ++c) {
                if (spectrum[c] <= halfMax) {
                    const double y1 = (c - 1 >= maxBin) ? spectrum[c - 1] : halfMax, y2 = spectrum[c];
                    rHM = (static_cast<double>(c) + 0.5) - ((std::abs(y1 - y2) > 1e-6) ? (halfMax - y2) / (y1 - y2) : 0.0);
                    break;
                }
            }
            const double obsFwhm = rHM - lHM;
            const double fwhm = (obsFwhm >= 0.6 && obsFwhm <= expFwhm * 2.2) ? obsFwhm : expFwhm;

            outBkg = bkg;
            outNet = net;
            outMomCh = momCh;
            outSigma = std::clamp(fwhm / 2.35482, 0.3, 15.0);
            const double estArea = net * outSigma * std::sqrt(2.0 * M_PI);
            const double effMinCounts = std::clamp(minCounts, 3.0, 15.0);
            return ((estArea >= effMinCounts || net >= effMinCounts) && std::abs(momCh - searchCenter) <= std::max(scanHalfWin, maxAllowedDriftCh));
        };

        const double nomCh = def.physicalEnergy / disp;
        double bkg = 0.0, net = 0.0, momCh = expCh, sigCh = std::clamp(expFwhm / 2.35482, 0.3, 15.0);

        // Multi-tier peak finding:
        // Tier 1: Search in configured window around expected channel (expCh)
        bool strongSeed = scanWindow(expCh, winCh, bkg, net, momCh, sigCh);

        // Tier 2: If missed and expected differs from base, search around base channel
        if (!strongSeed && std::abs(expCh - baseCh) > 0.5) {
            double bkg2 = 0.0, net2 = 0.0, momCh2 = baseCh, sigCh2 = sigCh;
            if (scanWindow(baseCh, winCh, bkg2, net2, momCh2, sigCh2)) {
                strongSeed = true;
                bkg = bkg2; net = net2; momCh = momCh2; sigCh = sigCh2;
            }
        }

        // Tier 3: Search around nominal energy channel (physicalEnergy / disp)
        if (!strongSeed && std::abs(expCh - nomCh) > 0.5) {
            double bkg3 = 0.0, net3 = 0.0, momCh3 = nomCh, sigCh3 = sigCh;
            if (scanWindow(nomCh, winCh, bkg3, net3, momCh3, sigCh3)) {
                strongSeed = true;
                bkg = bkg3; net = net3; momCh = momCh3; sigCh = sigCh3;
            }
        }

        // Tier 4: Expand search window up to maxAllowedDriftCh around expCh and nomCh
        const double wideWin = std::max(winCh, maxAllowedDriftCh);
        if (!strongSeed && wideWin > winCh) {
            double bkgW = 0.0, netW = 0.0, momChW = expCh, sigChW = sigCh;
            if (scanWindow(expCh, wideWin, bkgW, netW, momChW, sigChW)) {
                strongSeed = true;
                bkg = bkgW; net = netW; momCh = momChW; sigCh = sigChW;
            } else if (scanWindow(nomCh, wideWin, bkgW, netW, momChW, sigChW)) {
                strongSeed = true;
                bkg = bkgW; net = netW; momCh = momChW; sigCh = sigChW;
            }
        }

        BoundPeakSeed seed;
        seed.anchorIdx = static_cast<int>(aIdx);
        seed.energy = def.physicalEnergy;
        seed.energyErr = (def.energyError > 0.0) ? def.energyError : 0.05;
        seed.uNorm = def.physicalEnergy / disp;
        // Always include every anchor peak; if a single peak is weak in this run, seed it at expCh
        // with larger seed uncertainty so stronger peaks guide the initial polynomial seed.
        seed.seedCh = strongSeed ? momCh : expCh;
        const double estArea = std::max(10.0, net * sigCh * std::sqrt(2.0 * M_PI));
        const double bkgFactor = std::sqrt(1.0 + (2.0 * std::max(0.0, bkg) * 2.35482 * sigCh) / estArea);
        seed.seedErrCh = strongSeed ? std::clamp((sigCh / std::sqrt(estArea)) * bkgFactor, 0.02, 0.4) : 1.0;
        seed.seedSigmaCh = sigCh;
        seed.seedAmpl = std::max(1.0, net);
        seed.seedBkg = std::max(0.0, bkg);
        seed.windowCh = winCh;
        seed.expFwhmCh = expFwhm;
        activePeaks.push_back(seed);
    }

    const int nActive = static_cast<int>(activePeaks.size());
    const int minRequired = useQuadratic ? 3 : 2;
    if (nActive < minRequired) {
        outResult.fittedAnchors = allAnchors;
        outResult.status = RunCalibStatus::NoPeakInWindow;
        outResult.failureReason = QString("Only %1 anchor peaks configured (minimum %2 required).")
                                      .arg(nActive).arg(minRequired);
        return false;
    }

    // 2. Initial polynomial seeds for po(u) and s^2(u) (Xtrackn boundfit L4864-L4915)
    // Include energy uncertainty (energyErr / disp)^2 in the position weights so clustered low-energy
    // peaks cannot overpower high-energy anchor peaks during initial seeding.
    const int nE = (useQuadratic && nActive >= 3) ? 3 : 2;
    std::vector<double> uVec(nActive), chVec(nActive), wPosVec(nActive);
    std::vector<double> uScaledVec(nActive), s2Vec(nActive), wWidVec(nActive, 1.0);
    for (int i = 0; i < nActive; ++i) {
        uVec[i] = activePeaks[i].uNorm;
        chVec[i] = activePeaks[i].seedCh;
        const double eErrCh = activePeaks[i].energyErr / disp;
        const double sErrCh = activePeaks[i].seedErrCh;
        wPosVec[i] = 1.0 / (sErrCh * sErrCh + eErrCh * eErrCh);
        uScaledVec[i] = activePeaks[i].uNorm / 1000.0;
        s2Vec[i] = activePeaks[i].seedSigmaCh * activePeaks[i].seedSigmaCh;
        wWidVec[i] = std::max(1.0, activePeaks[i].seedAmpl);
    }

    std::vector<double> pCoeffs;
    if (!solveSmallPoly(uVec, chVec, wPosVec, nE, pCoeffs)) {
        pCoeffs = {0.0, 1.0};
        if (nE == 3) pCoeffs.push_back(0.0);
    }

    std::vector<double> wCoeffs;
    if (!solveSmallPoly(uScaledVec, s2Vec, wWidVec, 2, wCoeffs) || wCoeffs[1] < 0.0) {
        double sumW = 0.0, sumWS2 = 0.0;
        for (int i = 0; i < nActive; ++i) {
            sumW += wWidVec[i];
            sumWS2 += wWidVec[i] * s2Vec[i];
        }
        wCoeffs = {(sumW > 0.0) ? (sumWS2 / sumW) : s2Vec[0], 0.0};
    }
    double w0 = std::clamp(wCoeffs[0], 0.1, 100.0);
    double w1 = std::clamp(wCoeffs[1], 0.0, 25.0);

    // 3. Build initial regions and run Two-Pass Coupled Minimization (Xtrackn boundfit L4989-L5000)
    std::vector<BoundFitRegion> regions;
    buildBoundRegions(spectrum, snipBg, activePeaks, pCoeffs, w0, w1, regions);
    if (regions.empty()) {
        outResult.fittedAnchors = allAnchors;
        outResult.status = RunCalibStatus::NoPeakInWindow;
        outResult.failureReason = "Could not construct valid fit regions.";
        return false;
    }

    auto runCoupledMinimizerPass = [&](bool releaseWidths,
                                       std::vector<double> &ioPCoeffs,
                                       double &ioW0,
                                       double &ioW1,
                                       std::vector<double> &outRegionBkgs,
                                       std::vector<double> &outPeakAmpls,
                                       std::vector<double> &outPosCov) -> bool {
        buildBoundRegions(spectrum, snipBg, activePeaks, ioPCoeffs, ioW0, ioW1, regions);
        const int nReg = static_cast<int>(regions.size());
        if (nReg == 0) return false;

        CoupledBoundFCN fcn(spectrum, snipBg, activePeaks, regions, nE, disp);

        std::unique_ptr<ROOT::Math::Minimizer> minimizer(
            ROOT::Math::Factory::CreateMinimizer("Minuit2", "Migrad"));
        if (!minimizer) {
            minimizer.reset(ROOT::Math::Factory::CreateMinimizer("Minuit", "Migrad"));
        }
        if (!minimizer) return false;

        minimizer->SetMaxFunctionCalls(5000);
        minimizer->SetMaxIterations(2000);
        minimizer->SetTolerance(0.01);
        minimizer->SetPrintLevel(0);
        minimizer->SetErrorDef(1.0); // Deviance 2*NLL has ErrorDef = 1.0 for 1-sigma errors
        minimizer->SetFunction(fcn);

        // Global inverse energy polynomial parameters p_0, p_1 (, p_2)
        minimizer->SetLimitedVariable(0, "p0", ioPCoeffs[0], 0.05,
                                      ioPCoeffs[0] - maxAllowedDriftCh - 10.0,
                                      ioPCoeffs[0] + maxAllowedDriftCh + 10.0);
        minimizer->SetLimitedVariable(1, "p1", ioPCoeffs[1], 1e-4, 0.5, 2.0);
        if (nE >= 3) {
            minimizer->SetLimitedVariable(2, "p2", ioPCoeffs[2], 1e-8, -1e-3, 1e-3);
        }

        // Global width-squared polynomial parameters w_0 > 0, w_1 >= 0 (strictly non-decreasing width with energy)
        minimizer->SetLimitedVariable(nE,     "w0", std::clamp(ioW0, 0.08, 150.0), 0.05, 0.08, 150.0);
        minimizer->SetLimitedVariable(nE + 1, "w1", std::clamp(ioW1, 0.0, 30.0),   0.02, 0.0,  30.0);
        if (!releaseWidths) {
            minimizer->FixVariable(nE);
            minimizer->FixVariable(nE + 1);
        }

        // Region SNIP-scaled backgrounds B0_r (tightly bounded around SNIP continuum regions[r].initBkg)
        const int bkgOffset = nE + 2;
        for (int r = 0; r < nReg; ++r) {
            const double snipRef = regions[r].initBkg;
            double minPeakAmpl = 1e9;
            for (int pIdx : regions[r].peakIndices) {
                if (pIdx >= 0 && pIdx < nActive) {
                    minPeakAmpl = std::min(minPeakAmpl, activePeaks[pIdx].seedAmpl);
                }
            }
            if (minPeakAmpl > 1e8) minPeakAmpl = 10.0;

            const double minB = std::max(0.0, 0.85 * snipRef - 0.5);
            const double maxHeadroom = std::max(1.0, 0.15 * snipRef + 0.5 * std::sqrt(std::max(1.0, snipRef)));
            const double maxB = snipRef + std::min(maxHeadroom, std::max(0.5, 0.15 * minPeakAmpl));
            const double initB = (r < static_cast<int>(outRegionBkgs.size()) && outRegionBkgs[r] >= 0.0)
                                 ? std::clamp(outRegionBkgs[r], minB, maxB)
                                 : std::clamp(snipRef, minB, maxB);
            minimizer->SetLimitedVariable(bkgOffset + r,
                                          Form("B0_%d", r),
                                          initB,
                                          std::max(0.05, 0.03 * snipRef + 0.05),
                                          minB,
                                          maxB);
        }

        // Peak amplitudes A_i
        const int ampOffset = bkgOffset + nReg;
        for (int i = 0; i < nActive; ++i) {
            const int rIdx = activePeaks[i].regionIdx;
            const double regBkg = (rIdx >= 0 && rIdx < nReg) ? regions[rIdx].initBkg : activePeaks[i].seedBkg;
            const double u = activePeaks[i].uNorm;
            double po = ioPCoeffs[0] + ioPCoeffs[1] * u;
            if (nE >= 3) po += ioPCoeffs[2] * u * u;
            const int cBin = std::clamp(static_cast<int>(std::floor(po)), 1, totalBins - 2);
            const double initA = (i < static_cast<int>(outPeakAmpls.size()) && outPeakAmpls[i] > 0.0)
                                 ? outPeakAmpls[i]
                                 : std::max(1.0, std::max(activePeaks[i].seedAmpl, spectrum[cBin] - regBkg));
            const double maxA = std::max(20.0, initA * 4.0);
            minimizer->SetLimitedVariable(ampOffset + i,
                                          Form("A_%d", i),
                                          std::clamp(initA, 0.1, maxA),
                                          std::max(0.5, 0.05 * initA),
                                          0.0,
                                          maxA);
        }

        minimizer->Minimize();
        if (releaseWidths) {
            minimizer->Hesse();
        }

        const double *xs = minimizer->X();
        if (!xs) return false;

        for (int k = 0; k < nE; ++k) ioPCoeffs[k] = xs[k];
        ioW0 = xs[nE];
        ioW1 = xs[nE + 1];

        outRegionBkgs.resize(nReg);
        for (int r = 0; r < nReg; ++r) outRegionBkgs[r] = xs[bkgOffset + r];

        outPeakAmpls.resize(nActive);
        for (int i = 0; i < nActive; ++i) outPeakAmpls[i] = xs[ampOffset + i];

        outPosCov.assign(nE * nE, 0.0);
        for (int j = 0; j < nE; ++j) {
            for (int k = 0; k < nE; ++k) {
                outPosCov[j * nE + k] = minimizer->CovMatrix(j, k);
            }
        }
        return true;
    };

    std::vector<double> regionBkgs;
    std::vector<double> peakAmpls;
    std::vector<double> posCov;

    // Pass 1a: Freeze width polynomial (w0, w1) and optimize global positions + amplitudes + flat backgrounds
    runCoupledMinimizerPass(false, pCoeffs, w0, w1, regionBkgs, peakAmpls, posCov);
    // Pass 1b (TRSET dynamic window slide): Recenter windows around Pass-1 positions and refine
    regionBkgs.clear();
    runCoupledMinimizerPass(false, pCoeffs, w0, w1, regionBkgs, peakAmpls, posCov);
    // Pass 2: Unfreeze width polynomial (w0, w1) and jointly refine positions, widths, amplitudes, and backgrounds
    runCoupledMinimizerPass(true, pCoeffs, w0, w1, regionBkgs, peakAmpls, posCov);

    // Rebuild final regions at converged parameters
    buildBoundRegions(spectrum, snipBg, activePeaks, pCoeffs, w0, w1, regions);

    // 4. Populate FittedAnchors from the converged coupled parameters
    for (int i = 0; i < nActive; ++i) {
        const auto &seed = activePeaks[i];
        const double u = seed.uNorm;
        double boundPo = pCoeffs[0] + pCoeffs[1] * u;
        if (nE >= 3) boundPo += pCoeffs[2] * u * u;

        const double s2 = std::clamp(std::abs(w0 + w1 * (u / 1000.0)), 0.09, 250.0);
        const double sigma = std::sqrt(s2);
        const double fwhm = 2.35482 * sigma;
        const int rIdx = seed.regionIdx;
        const double bkgMean = (rIdx >= 0 && rIdx < static_cast<int>(regionBkgs.size()))
                               ? regionBkgs[rIdx] : seed.seedBkg;
        const bool useSnipChord = (rIdx >= 0 && rIdx < static_cast<int>(regions.size()) && regions[rIdx].meanSnip > 0.25);
        const double xCenterReg = useSnipChord ? regions[rIdx].xCenterReg : boundPo;
        const double slopeSnip = useSnipChord ? regions[rIdx].slopeSnip : 0.0;
        const int centBin = std::clamp(static_cast<int>(std::floor(boundPo)), 1, totalBins - 2);
        const double bkgAtPeak = useSnipChord
            ? std::max(0.0, bkgMean + slopeSnip * (boundPo - xCenterReg))
            : bkgMean;
        const double ampl = (i < static_cast<int>(peakAmpls.size()))
                            ? peakAmpls[i] : seed.seedAmpl;
        const double area = ampl * sigma * std::sqrt(2.0 * M_PI);

        // Compute local Pearson reduced Chi2/ndf across this peak's +- 2.5*FWHM window
        const int maxHalfWin = std::max(6, static_cast<int>(std::round(seed.windowCh * 1.5)));
        const int hw = std::clamp(static_cast<int>(std::round(2.5 * fwhm)), 5, maxHalfWin);
        const int c1 = std::max(1, centBin - hw);
        const int c2 = std::min(totalBins - 2, centBin + hw);
        double localChi2 = 0.0;
        int nBins = 0;
        for (int ch = c1; ch <= c2; ++ch) {
            const double xCenter = static_cast<double>(ch) + 0.5;
            double yMod = useSnipChord
                ? std::max(0.0, bkgMean + slopeSnip * (xCenter - xCenterReg))
                : bkgMean;
            if (rIdx >= 0 && rIdx < static_cast<int>(regions.size())) {
                for (int pIdx : regions[rIdx].peakIndices) {
                    const double uOther = activePeaks[pIdx].uNorm;
                    double poOther = pCoeffs[0] + pCoeffs[1] * uOther;
                    if (nE >= 3) poOther += pCoeffs[2] * uOther * uOther;
                    const double s2Other = std::clamp(std::abs(w0 + w1 * (uOther / 1000.0)), 0.09, 250.0);
                    const double aOther = (pIdx < static_cast<int>(peakAmpls.size())) ? peakAmpls[pIdx] : activePeaks[pIdx].seedAmpl;
                    const double dx = xCenter - poOther;
                    yMod += aOther * std::exp(-0.5 * dx * dx / s2Other);
                }
            } else {
                const double dx = xCenter - boundPo;
                yMod += ampl * std::exp(-0.5 * dx * dx / s2);
            }
            const double obs = spectrum[ch];
            const double var = std::max(1.0, obs);
            localChi2 += (obs - yMod) * (obs - yMod) / var;
            nBins++;
        }
        const double peakChi2Ndf = (nBins > 3) ? (localChi2 / static_cast<double>(nBins - 3)) : 1.0;

        // Statistical centroid error (Xtrackn TRACKFIT L4563: dxfit = sqrt(s2/ar) with background factor)
        const double statErr = (area > 1.0) ? (sigma / std::sqrt(area)) : 0.08;
        const double bkgFactor = (area > 1.0 && bkgAtPeak > 0.0)
            ? std::sqrt(1.0 + (2.0 * bkgAtPeak * fwhm) / area)
            : 1.0;
        double centErr = std::clamp(statErr * bkgFactor, 0.02, 0.40);

        // Refine observed peak centroid from local continuum-subtracted data around boundPo (+- 1.8*sigma)
        const int c1Mom = std::max(1, static_cast<int>(std::floor(boundPo - 1.8 * sigma)));
        const int c2Mom = std::min(totalBins - 2, static_cast<int>(std::ceil(boundPo + 1.8 * sigma)));
        double sumObsNet = 0.0, sumObsNetX = 0.0;
        for (int ch = c1Mom; ch <= c2Mom; ++ch) {
            const double xCenter = static_cast<double>(ch) + 0.5;
            const double bgVal = useSnipChord
                ? std::max(0.0, bkgMean + slopeSnip * (xCenter - xCenterReg))
                : bkgMean;
            const double netVal = std::max(0.0, spectrum[ch] - bgVal);
            sumObsNet += netVal;
            sumObsNetX += netVal * xCenter;
        }
        double refinedCentroid = boundPo;
        if (sumObsNet > 3.0) {
            const double obsCh = sumObsNetX / sumObsNet;
            refinedCentroid = std::clamp(obsCh, boundPo - sigma, boundPo + sigma);
        }

        const int aIdx = seed.anchorIdx;

        FittedAnchor fa;
        fa.energy = seed.energy;
        fa.energyErr = seed.energyErr;
        fa.centroidCh = refinedCentroid;
        fa.centroidErrCh = centErr;
        fa.fwhmCh = fwhm;
        fa.amplitude = ampl;
        fa.background = bkgAtPeak;
        fa.area = area;
        fa.chi2 = peakChi2Ndf;
        fa.fitWinCh = static_cast<double>(hw);
        // All configured anchor peaks remain included (isValid = true);
        // peak exclusion only happens if the user manually unchecks a peak in the Intervention Dialog.
        fa.isValid = true;
        allAnchors[aIdx] = fa;
    }

    outResult.fittedAnchors = allAnchors;

    // 5. Check run-level quality conditions BEFORE computing calibration.
    // If any quality check fails, explicitly reset outResult coefficients to identity (0.0, 1.0, 0.0).
    int nFittedAboveMinCounts = 0;
    for (int i = 0; i < nActive; ++i) {
        const auto &seed = activePeaks[i];
        const auto &fa = allAnchors[seed.anchorIdx];
        const double minC = (seed.anchorIdx < static_cast<int>(anchors.size()))
                            ? std::max(1.0, anchors[seed.anchorIdx].minCounts) : 5.0;
        if (fa.area >= minC || fa.amplitude >= minC) {
            nFittedAboveMinCounts++;
        }
    }
    if (nFittedAboveMinCounts < minRequired) {
        outResult.a0 = 0.0;
        outResult.a1 = 1.0;
        outResult.a2 = 0.0;
        outResult.numCoeffs = useQuadratic ? 3 : 2;
        outResult.status = RunCalibStatus::LowStatistics;
        outResult.failureReason = QString("Low statistics: only %1 of %2 anchor peaks met minimum area/counts (required: %3).")
                                      .arg(nFittedAboveMinCounts).arg(nAnchors).arg(minRequired);
        return false;
    }
    for (int i = 0; i < nActive; ++i) {
        const auto &seed = activePeaks[i];
        const auto &fa = allAnchors[seed.anchorIdx];
        const double baseCh = (seed.anchorIdx < static_cast<int>(baseCentroids.size()))
                              ? baseCentroids[seed.anchorIdx] : seed.uNorm;
        const double expCh = (seed.anchorIdx < static_cast<int>(initialExpectedChannels.size()))
                             ? initialExpectedChannels[seed.anchorIdx] : baseCh;
        const double trackedDrift = std::min(std::abs(fa.centroidCh - expCh), std::abs(fa.centroidCh - baseCh));
        if (trackedDrift > maxAllowedDriftCh) {
            outResult.a0 = 0.0;
            outResult.a1 = 1.0;
            outResult.a2 = 0.0;
            outResult.numCoeffs = useQuadratic ? 3 : 2;
            outResult.status = RunCalibStatus::ExcessiveDrift;
            outResult.failureReason = QString("Excessive drift: anchor %1 keV shifted by %2 ch from expected %3 ch (max allowed %4 ch).")
                                          .arg(seed.energy, 0, 'f', 1)
                                          .arg(trackedDrift, 0, 'f', 2)
                                          .arg(expCh, 0, 'f', 1)
                                          .arg(maxAllowedDriftCh, 0, 'f', 1);
            return false;
        }
        if (fa.fwhmCh < 0.4 || fa.fwhmCh > seed.expFwhmCh * 2.5) {
            outResult.a0 = 0.0;
            outResult.a1 = 1.0;
            outResult.a2 = 0.0;
            outResult.numCoeffs = useQuadratic ? 3 : 2;
            outResult.status = RunCalibStatus::BadFWHM;
            outResult.failureReason = QString("Bad FWHM: anchor %1 keV fitted FWHM (%2 ch) is outside physical bounds [0.4, %3 ch].")
                                          .arg(seed.energy, 0, 'f', 1)
                                          .arg(fa.fwhmCh, 0, 'f', 2)
                                          .arg(seed.expFwhmCh * 2.5, 0, 'f', 1);
            return false;
        }
    }

    // 6. Invert the observed anchor centroids into forward calibration E(ch)/disp = a0 + a1*ch (+ a2*ch^2)
    // (Xtrackn TRACKFIT L4554-L4579)
    bool calibOk = computeCalibration(allAnchors, useQuadratic, outResult, maxAllowedChi2NDF, disp);
    if (!calibOk) {
        outResult.a0 = 0.0;
        outResult.a1 = 1.0;
        outResult.a2 = 0.0;
        outResult.numCoeffs = useQuadratic ? 3 : 2;
        return false;
    }

    return true;
}

//==============================================================================
// computeCalibration
//==============================================================================
bool RunByRunEngine::computeCalibration(const std::vector<FittedAnchor> &anchors,
                                       bool useQuadratic,
                                       RunCalibResult &outResult,
                                       double maxAllowedChi2NDF,
                                       double nominalDispersion)
{
    std::vector<FittedAnchor> valid;
    for (const auto &a : anchors) {
        if (a.isValid) valid.push_back(a);
    }

    if (valid.size() < 2) {
        outResult.status = RunCalibStatus::NoPeakInWindow;
        outResult.failureReason = "Less than 2 valid anchor peaks found.";
        return false;
    }

    outResult.fittedAnchors = valid;

    const double disp = (nominalDispersion > 0.0) ? nominalDispersion
                      : ((outResult.nominalGain > 0.0) ? outResult.nominalGain : 1.0);
    outResult.nominalGain = disp;

    // Average spectrum-fit reduced Chi2/ndf across active anchors (Xtrackn boundfit goodness-of-fit)
    double sumSpecChi2 = 0.0;
    int nSpecChi2 = 0;
    for (const auto &a : valid) {
        if (a.chi2 > 0.0) {
            sumSpecChi2 += a.chi2;
            nSpecChi2++;
        }
    }
    const double avgSpecChi2 = (nSpecChi2 > 0) ? (sumSpecChi2 / static_cast<double>(nSpecChi2)) : 0.0;

    // Standard Linear Calibration: targetCh = a0 + a1 * ch (where targetCh = energy / disp)
    // Gains a1 are dimensionless relative gain correction factors (~1.0), maintaining the nominal dispersion.
    if (!useQuadratic || valid.size() < 3) {
        outResult.numCoeffs = 2;
        double sumW = 0.0, sumWX = 0.0, sumWY = 0.0, sumWXX = 0.0, sumWXY = 0.0;

        for (const auto &a : valid) {
            const double targetCh = a.energy / disp;
            const double targetErrCh = (a.energyErr > 0.0 ? a.energyErr : 0.05) / disp;
            const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
            const double sigTotSq = std::max(0.0025, (targetErrCh * targetErrCh) + (centErr * centErr));
            const double w = 1.0 / sigTotSq;
            sumW += w;
            sumWX += w * a.centroidCh;
            sumWY += w * targetCh;
            sumWXX += w * a.centroidCh * a.centroidCh;
            sumWXY += w * a.centroidCh * targetCh;
        }

        const double delta = sumW * sumWXX - sumWX * sumWX;
        if (std::abs(delta) < 1e-12) {
            outResult.status = RunCalibStatus::FileError;
            outResult.failureReason = "Singular matrix in linear regression.";
            return false;
        }

        outResult.a0 = (sumWXX * sumWY - sumWX * sumWXY) / delta;
        outResult.a1 = (sumW * sumWXY - sumWX * sumWY) / delta;
        outResult.a2 = 0.0;

        outResult.sigmaA0 = std::sqrt(std::max(0.0, sumWXX / delta));
        outResult.sigmaA1 = std::sqrt(std::max(0.0, sumW / delta));
        outResult.sigmaA2 = 0.0;
        outResult.covA0A1 = -sumWX / delta;

        // Residual (in keV) and dimensionless Chi2 calculation
        double sumResSq = 0.0, sumChi2 = 0.0;
        for (const auto &a : valid) {
            const double targetCh = a.energy / disp;
            const double fitCh = outResult.a0 + outResult.a1 * a.centroidCh;
            const double resCh = targetCh - fitCh;
            const double resE = resCh * disp;
            sumResSq += resE * resE;

            const double targetErrCh = (a.energyErr > 0.0 ? a.energyErr : 0.05) / disp;
            const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
            const double sigCh = std::sqrt(std::max(0.0025, (targetErrCh * targetErrCh) +
                                                          (outResult.a1 * outResult.a1 * centErr * centErr)));
            sumChi2 += (resCh * resCh) / (sigCh * sigCh);
        }

        const int ndf = std::max(1, static_cast<int>(valid.size()) - 2);
        outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
        const double lineChi2Ndf = sumChi2 / static_cast<double>(ndf);
        // In boundfit (or 2-point linear fit), centroids lie directly on the polynomial so lineChi2Ndf == 0;
        // use std::max(lineChi2Ndf, avgSpecChi2) so both batch matrix and live inspection report the same Chi2/ndf.
        outResult.chi2NDF = (lineChi2Ndf > 1e-4) ? std::max(lineChi2Ndf, avgSpecChi2) : avgSpecChi2;

        if (outResult.chi2NDF > maxAllowedChi2NDF || std::isnan(outResult.chi2NDF)) {
            outResult.status = RunCalibStatus::BadChi2;
            outResult.failureReason = QString("Reduced Chi2/ndf (%1) exceeds acceptable cutoff of %2.")
                                          .arg(outResult.chi2NDF, 0, 'f', 2)
                                          .arg(maxAllowedChi2NDF, 0, 'f', 1);
            return false;
        }

        outResult.status = RunCalibStatus::Success;
        outResult.failureReason.clear();
        return true;
    }

    // Quadratic Calibration: targetCh = a0 + a1 * ch + a2 * ch^2
    outResult.numCoeffs = 3;
    double m[3][3] = {{0.0}};
    double b[3] = {0.0};

    for (const auto &a : valid) {
        const double targetCh = a.energy / disp;
        const double targetErrCh = (a.energyErr > 0.0 ? a.energyErr : 0.05) / disp;
        const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
        const double sigTotSq = std::max(0.0025, (targetErrCh * targetErrCh) + (centErr * centErr));
        const double w = 1.0 / sigTotSq;
        const double x = a.centroidCh;
        const double x2 = x * x;
        const double x3 = x2 * x;
        const double x4 = x2 * x2;
        const double y = targetCh;

        m[0][0] += w;       m[0][1] += w * x;    m[0][2] += w * x2;
        m[1][0] += w * x;   m[1][1] += w * x2;   m[1][2] += w * x3;
        m[2][0] += w * x2;  m[2][1] += w * x3;   m[2][2] += w * x4;

        b[0] += w * y;
        b[1] += w * x * y;
        b[2] += w * x2 * y;
    }

    // 3x3 Determinant and Inversion
    const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    if (std::abs(det) < 1e-15) {
        // Fallback to linear
        return computeCalibration(anchors, false, outResult, maxAllowedChi2NDF, disp);
    }

    const double invDet = 1.0 / det;
    double inv[3][3];
    inv[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
    inv[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

    outResult.a0 = inv[0][0] * b[0] + inv[0][1] * b[1] + inv[0][2] * b[2];
    outResult.a1 = inv[1][0] * b[0] + inv[1][1] * b[1] + inv[1][2] * b[2];
    outResult.a2 = inv[2][0] * b[0] + inv[2][1] * b[1] + inv[2][2] * b[2];

    outResult.sigmaA0 = std::sqrt(std::max(0.0, inv[0][0]));
    outResult.sigmaA1 = std::sqrt(std::max(0.0, inv[1][1]));
    outResult.sigmaA2 = std::sqrt(std::max(0.0, inv[2][2]));
    outResult.covA0A1 = inv[0][1];

    double sumResSq = 0.0, sumChi2 = 0.0;
    for (const auto &a : valid) {
        const double targetCh = a.energy / disp;
        const double fitCh = outResult.a0 + outResult.a1 * a.centroidCh + outResult.a2 * a.centroidCh * a.centroidCh;
        const double resCh = targetCh - fitCh;
        const double resE = resCh * disp;
        sumResSq += resE * resE;

        const double targetErrCh = (a.energyErr > 0.0 ? a.energyErr : 0.05) / disp;
        const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
        const double dEdx = std::abs(outResult.a1 + 2.0 * outResult.a2 * a.centroidCh);
        const double sigCh = std::sqrt(std::max(0.0025, (targetErrCh * targetErrCh) +
                                                      (dEdx * dEdx * centErr * centErr)));
        sumChi2 += (resCh * resCh) / (sigCh * sigCh);
    }

    const int ndf = std::max(1, static_cast<int>(valid.size()) - 3);
    outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
    const double lineChi2Ndf = sumChi2 / static_cast<double>(ndf);
    outResult.chi2NDF = (lineChi2Ndf > 1e-4) ? std::max(lineChi2Ndf, avgSpecChi2) : avgSpecChi2;

    if (outResult.chi2NDF > maxAllowedChi2NDF || std::isnan(outResult.chi2NDF)) {
        outResult.status = RunCalibStatus::BadChi2;
        outResult.failureReason = QString("Reduced Chi2/ndf (%1) exceeds acceptable cutoff of %2.")
                                      .arg(outResult.chi2NDF, 0, 'f', 2)
                                      .arg(maxAllowedChi2NDF, 0, 'f', 1);
        return false;
    }

    outResult.status = RunCalibStatus::Success;
    outResult.failureReason.clear();
    return true;
}

void RunByRunEngine::setChunkedMode(bool enabled, int chunkSize, double jumpCutoff, bool applySmoothing,
                                    const std::vector<DetectorTrajectory> &pass1Traj)
{
    m_isChunkedRun = enabled;
    m_chunkSize = std::max(2, chunkSize);
    m_jumpCutoff = (jumpCutoff > 0.0) ? jumpCutoff : 0.00035;
    m_chunkApplySmoothing = applySmoothing;
    if (!pass1Traj.empty()) {
        m_pass1Trajectories = pass1Traj;
    }
}

//==============================================================================
// run: Dispatches to either single-run pass or chunked pass
//==============================================================================
void RunByRunEngine::run()
{
    if (m_isChunkedRun) {
        runChunkedPass();
    } else {
        runSingleRunPass();
    }
}

//==============================================================================
// runSingleRunPass: Detector-by-Detector Trajectory Tracking Loop (Pass 1)
//==============================================================================
void RunByRunEngine::runSingleRunPass()
{
    m_stopRequested.store(false);
    m_trajectories.clear();

    const int nRuns = static_cast<int>(m_config.runFilePaths.size());
    const int nDets = m_config.detectorEnd - m_config.detectorStart + 1;
    if (nRuns <= 0 || nDets <= 0 || m_config.anchors.empty()) {
        emit batchFinished(false, 0, 0);
        return;
    }

    const double dispersion = (m_config.nominalDispersion > 0.0) ? m_config.nominalDispersion : 1.0;

    int overallSuccessCount = 0;
    int overallFailureCount = 0;
    int processedSteps = 0;
    const int totalSteps = nDets * nRuns;

    // OUTER LOOP: Detector d in [detectorStart .. detectorEnd]
    for (int detId = m_config.detectorStart; detId <= m_config.detectorEnd; ++detId) {
        if (m_stopRequested.load()) break;

        emit detectorStarted(detId, nDets);

        DetectorTrajectory traj;
        traj.detectorId = detId;

        // Initialize base centroids for all anchors: initialChannel or physicalEnergy / dispersion
        std::vector<double> baseCentroids;
        double sumExpFwhm = 0.0;
        for (const auto &anchorDef : m_config.anchors) {
            double guessCh = (anchorDef.initialChannel > 0.0)
                ? anchorDef.initialChannel
                : (anchorDef.physicalEnergy / dispersion);
            baseCentroids.push_back(guessCh);
            sumExpFwhm += std::max(1.5, anchorDef.expectedFwhmCh);
        }
        traj.lastValidCentroids = baseCentroids;
        const int snipIters = std::clamp(
            static_cast<int>(std::round(2.5 * (sumExpFwhm / static_cast<double>(m_config.anchors.size())))),
            12, 35);
        std::vector<double> lastValidPeakSpectrum;

        // 2. INNER LOOP: Run r in [0 .. nRuns - 1]
        for (int rIdx = 0; rIdx < nRuns; ++rIdx) {
            if (m_stopRequested.load()) break;

            const QString runPath = m_config.runFilePaths[rIdx];
            QFileInfo runInfo(runPath);

            int runNum = rIdx + 1;
            int lastDot = runInfo.fileName().lastIndexOf('.');
            if (lastDot > 0) {
                bool ok = false;
                int parsed = runInfo.fileName().mid(lastDot + 1).toInt(&ok);
                if (ok) runNum = parsed;
            }

            RunCalibResult res;
            res.runNumber = runNum;
            res.runFileName = runInfo.fileName();
            res.fullFilePath = runPath;
            res.detectorId = detId;
            res.nominalGain = dispersion;

            // Load detector d spectrum for run r
            std::vector<double> runSpectrum;
            QString err;
            if (!ReadSpectrumData(runPath.toStdString(),
                                 m_config.spectrumFormat,
                                 m_config.spectrumChannels,
                                 detId,
                                 runSpectrum,
                                 &err) || runSpectrum.empty()) {
                res.status = RunCalibStatus::FileError;
                res.failureReason = QString("Could not read spectrum from run file: %1").arg(err);
                traj.runResults.push_back(res);
                traj.failureCount++;
                overallFailureCount++;
                processedSteps++;
                emit runProcessed(detId, rIdx, nRuns, res);
                continue;
            }

            // Extract SNIP Compton continuum background & continuum-subtracted peak spectrum
            std::vector<double> snipBg = computeSnipContinuum(runSpectrum, snipIters);
            std::vector<double> currPeakSpectrum(runSpectrum.size(), 0.0);
            for (size_t c = 0; c < runSpectrum.size(); ++c) {
                currPeakSpectrum[c] = std::max(0.0, runSpectrum[c] - snipBg[c]);
            }

            // Whole-spectrum cross-correlation gain/shift pre-alignment against previous valid run (Xtrackn gainfit)
            std::vector<double> expectedCentroids = traj.lastValidCentroids;
            if (!lastValidPeakSpectrum.empty()) {
                preAlignGainFit(lastValidPeakSpectrum,
                                currPeakSpectrum,
                                traj.lastValidCentroids,
                                m_config.maxAllowedDriftCh,
                                expectedCentroids);
            }

            // Simultaneous coupled multi-peak spectrum fit (Xtrackn boundfit + CERN ROOT Minimizer + SNIP Continuum)
            const bool calibSuccess = fitSpectrumBound(runSpectrum,
                                                       m_config.anchors,
                                                       expectedCentroids,
                                                       baseCentroids,
                                                       m_config.useQuadratic,
                                                       dispersion,
                                                       m_config.maxAllowedDriftCh,
                                                       m_config.maxAllowedChi2NDF,
                                                       res,
                                                       &snipBg);

            if (calibSuccess) {
                res.status = RunCalibStatus::Success;
                res.isFallbackFromPrevious = false;
                // Update moving anchor centroids and reference peak spectrum from the converged valid run
                for (size_t aIdx = 0; aIdx < res.fittedAnchors.size() && aIdx < traj.lastValidCentroids.size(); ++aIdx) {
                    if (res.fittedAnchors[aIdx].isValid && res.fittedAnchors[aIdx].centroidCh > 0.0) {
                        traj.lastValidCentroids[aIdx] = res.fittedAnchors[aIdx].centroidCh;
                    }
                }
                lastValidPeakSpectrum.swap(currPeakSpectrum);
                traj.runResults.push_back(res);
                traj.successCount++;
                overallSuccessCount++;
            } else {
                res.isFallbackFromPrevious = false;
                traj.runResults.push_back(res);
                traj.failureCount++;
                overallFailureCount++;
            }

            processedSteps++;
            const int overallPct = (processedSteps * 100) / std::max(1, totalSteps);
            emit runProcessed(detId, rIdx, nRuns, res);
            emit progressUpdated(overallPct, QString("Aligning Detector #%1: Run %2 of %3 (%4, %5 keV/ch)")
                                                 .arg(detId).arg(rIdx + 1).arg(nRuns).arg(runInfo.fileName())
                                                 .arg(dispersion, 0, 'f', 1));
        }

        traj.rawRunResults = traj.runResults;
        emit detectorFinished(detId, traj.successCount, traj.failureCount);
        m_trajectories.push_back(traj);
    }

    const bool wasCancelled = m_stopRequested.load();
    emit batchFinished(wasCancelled, overallSuccessCount, overallFailureCount);
}

//==============================================================================
// runChunkedPass: High-Statistics Raw Histogram Chunking bounded by Jumps
//==============================================================================
void RunByRunEngine::runChunkedPass()
{
    m_stopRequested.store(false);
    m_trajectories.clear();

    const int nRuns = static_cast<int>(m_config.runFilePaths.size());
    const int nDets = m_config.detectorEnd - m_config.detectorStart + 1;
    if (nRuns <= 0 || nDets <= 0 || m_config.anchors.empty() || m_pass1Trajectories.empty()) {
        emit batchFinished(false, 0, 0);
        return;
    }

    const double dispersion = (m_config.nominalDispersion > 0.0) ? m_config.nominalDispersion : 1.0;
    const int K = std::max(2, m_chunkSize);
    const double jumpCut = (m_jumpCutoff > 0.0) ? m_jumpCutoff : 0.00035;

    int overallSuccessCount = 0;
    int overallFailureCount = 0;
    int processedSteps = 0;
    const int totalSteps = nDets * nRuns;

    // Base centroids
    std::vector<double> baseCentroids;
    double sumExpFwhm = 0.0;
    for (const auto &anchorDef : m_config.anchors) {
        double guessCh = (anchorDef.initialChannel > 0.0)
            ? anchorDef.initialChannel
            : (anchorDef.physicalEnergy / dispersion);
        baseCentroids.push_back(guessCh);
        sumExpFwhm += std::max(1.5, anchorDef.expectedFwhmCh);
    }
    const int snipIters = std::clamp(
        static_cast<int>(std::round(2.5 * (sumExpFwhm / static_cast<double>(m_config.anchors.size())))),
        12, 35);

    // OUTER LOOP: Detector d in [detectorStart .. detectorEnd]
    for (int detId = m_config.detectorStart; detId <= m_config.detectorEnd; ++detId) {
        if (m_stopRequested.load()) break;

        emit detectorStarted(detId, nDets);

        // Find Pass 1 trajectory for this detector
        const DetectorTrajectory *pass1Traj = nullptr;
        for (const auto &t : m_pass1Trajectories) {
            if (t.detectorId == detId) {
                pass1Traj = &t;
                break;
            }
        }

        DetectorTrajectory traj;
        traj.detectorId = detId;
        traj.isChunked = true;
        traj.chunkSize = K;

        // If no Pass 1 trajectory or all runs failed, skip dead detector
        if (!pass1Traj || pass1Traj->runResults.empty() || pass1Traj->successCount == 0) {
            if (pass1Traj) {
                traj = *pass1Traj;
                traj.isChunked = true;
                traj.chunkSize = K;
            }
            m_trajectories.push_back(traj);
            processedSteps += nRuns;
            emit progressUpdated((processedSteps * 100) / totalSteps, QString("Skipping uncalibrated Detector #%1").arg(detId));
            emit detectorFinished(detId, 0, nRuns);
            continue;
        }

        traj.rawRunResults = pass1Traj->runResults;
        traj.lastValidCentroids = pass1Traj->lastValidCentroids;

        // Step 1: Identify jump boundaries across consecutive valid runs in Pass 1
        std::vector<bool> isJumpBefore(nRuns, false);
        int prevValid = -1;
        for (int i = 0; i < nRuns; ++i) {
            if (i >= static_cast<int>(pass1Traj->runResults.size())) break;
            const auto &res = pass1Traj->runResults[i];
            if (res.status != RunCalibStatus::Success || res.a1 <= 0.0) continue;
            if (prevValid >= 0) {
                double dGain = std::abs(res.a1 - pass1Traj->runResults[prevValid].a1);
                if (dGain >= jumpCut) {
                    isJumpBefore[i] = true;
                }
            }
            prevValid = i;
        }

        // Step 2: Partition runs into segments bounded by jumps
        struct RunSegment {
            int startRun{0};
            int endRun{0};
        };
        std::vector<RunSegment> segments;
        int segStart = 0;
        for (int i = 1; i < nRuns; ++i) {
            if (isJumpBefore[i]) {
                segments.push_back({segStart, i - 1});
                segStart = i;
            }
        }
        segments.push_back({segStart, nRuns - 1});

        // Step 3: Within each segment, form chunks of size K, sum raw spectra, and fit
        struct ChunkResult {
            int chunkId{0};
            std::vector<int> runIndices;
            double centerRunIndex{0.0};
            RunCalibResult fitRes;
            bool success{false};
        };

        std::vector<ChunkResult> allChunks;
        int globalChunkId = 0;

        for (const auto &seg : segments) {
            int r = seg.startRun;
            while (r <= seg.endRun) {
                int cEnd = std::min(seg.endRun, r + K - 1);
                ChunkResult chunk;
                chunk.chunkId = globalChunkId++;
                double sumR = 0.0;
                for (int cr = r; cr <= cEnd; ++cr) {
                    chunk.runIndices.push_back(cr);
                    sumR += static_cast<double>(cr);
                }
                chunk.centerRunIndex = sumR / static_cast<double>(chunk.runIndices.size());

                // Sum raw spectra for runs in chunk
                std::vector<double> chunkSpec(m_config.spectrumChannels, 0.0);
                int validRawReadCount = 0;
                for (int cr : chunk.runIndices) {
                    if (cr >= static_cast<int>(m_config.runFilePaths.size())) continue;
                    std::vector<double> rSpec;
                    QString rErr;
                    if (ReadSpectrumData(m_config.runFilePaths[cr].toStdString(),
                                         m_config.spectrumFormat,
                                         m_config.spectrumChannels,
                                         detId,
                                         rSpec,
                                         &rErr) && !rSpec.empty()) {
                        for (int ch = 0; ch < m_config.spectrumChannels && ch < static_cast<int>(rSpec.size()); ++ch) {
                            chunkSpec[ch] += rSpec[ch];
                        }
                        validRawReadCount++;
                    }
                }

                if (validRawReadCount > 0) {
                    // Compute SNIP continuum for chunk
                    auto snipBg = computeSnipContinuum(chunkSpec, snipIters);

                    // Use centroid from Pass 1 nearest to centerRunIndex as expectedCentroids
                    int centerR = static_cast<int>(std::round(chunk.centerRunIndex));
                    std::vector<double> expectedCentroids = traj.lastValidCentroids;
                    if (pass1Traj && centerR >= 0 && centerR < static_cast<int>(pass1Traj->runResults.size())) {
                        const auto &refRes = pass1Traj->runResults[centerR];
                        for (size_t aIdx = 0; aIdx < refRes.fittedAnchors.size() && aIdx < expectedCentroids.size(); ++aIdx) {
                            if (refRes.fittedAnchors[aIdx].isValid && refRes.fittedAnchors[aIdx].centroidCh > 0.0) {
                                expectedCentroids[aIdx] = refRes.fittedAnchors[aIdx].centroidCh;
                            }
                        }
                    }

                    RunCalibResult cRes;
                    cRes.detectorId = detId;
                    cRes.runNumber = centerR + 1;
                    cRes.runFileName = QString("Chunk_%1").arg(chunk.chunkId);
                    cRes.nominalGain = dispersion;

                    const bool calOk = fitSpectrumBound(chunkSpec,
                                                        m_config.anchors,
                                                        expectedCentroids,
                                                        baseCentroids,
                                                        m_config.useQuadratic,
                                                        dispersion,
                                                        m_config.maxAllowedDriftCh,
                                                        m_config.maxAllowedChi2NDF,
                                                        cRes,
                                                        &snipBg);
                    if (calOk) {
                        cRes.status = RunCalibStatus::Success;
                        chunk.fitRes = cRes;
                        chunk.success = true;
                        // Update last valid centroids
                        for (size_t aIdx = 0; aIdx < cRes.fittedAnchors.size() && aIdx < traj.lastValidCentroids.size(); ++aIdx) {
                            if (cRes.fittedAnchors[aIdx].isValid && cRes.fittedAnchors[aIdx].centroidCh > 0.0) {
                                traj.lastValidCentroids[aIdx] = cRes.fittedAnchors[aIdx].centroidCh;
                            }
                        }
                    } else {
                        chunk.fitRes = cRes;
                        chunk.success = false;
                    }
                }

                allChunks.push_back(chunk);
                r = cEnd + 1;
            }
        }

        // Step 4: Map chunk results back to each individual run within segments
        traj.runResults.resize(nRuns);
        int detSuccess = 0;
        int detFail = 0;

        for (const auto &seg : segments) {
            // Collect chunks belonging to this segment
            std::vector<ChunkResult*> segChunks;
            for (auto &ck : allChunks) {
                if (!ck.runIndices.empty() && ck.runIndices.front() >= seg.startRun && ck.runIndices.back() <= seg.endRun) {
                    segChunks.push_back(&ck);
                }
            }

            // Assign calibration to each run in segment
            for (int rIdx = seg.startRun; rIdx <= seg.endRun; ++rIdx) {
                RunCalibResult runRes;
                if (pass1Traj && rIdx < static_cast<int>(pass1Traj->runResults.size())) {
                    runRes = pass1Traj->runResults[rIdx];
                    runRes.rawA0 = pass1Traj->runResults[rIdx].a0;
                    runRes.rawA1 = pass1Traj->runResults[rIdx].a1;
                    runRes.hasRawA1 = true;
                } else {
                    runRes.runNumber = rIdx + 1;
                    runRes.runFileName = QFileInfo(m_config.runFilePaths[rIdx]).fileName();
                    runRes.fullFilePath = m_config.runFilePaths[rIdx];
                    runRes.detectorId = detId;
                    runRes.nominalGain = dispersion;
                }

                runRes.isJumpBoundary = isJumpBefore[rIdx];

                // Find chunk containing rIdx
                ChunkResult *parentChunk = nullptr;
                for (auto *ck : segChunks) {
                    for (int cr : ck->runIndices) {
                        if (cr == rIdx) {
                            parentChunk = ck;
                            break;
                        }
                    }
                    if (parentChunk) break;
                }

                if (parentChunk) {
                    runRes.chunkId = parentChunk->chunkId;
                    runRes.isChunkBoundary = (rIdx == parentChunk->runIndices.front());
                }

                double finA1 = 1.0;
                double finA0 = 0.0;
                double finA2 = 0.0;
                bool ok = false;

                if (m_chunkApplySmoothing && segChunks.size() > 1) {
                    double rD = static_cast<double>(rIdx);
                    if (rD <= segChunks.front()->centerRunIndex) {
                        if (segChunks.front()->success) {
                            finA1 = segChunks.front()->fitRes.a1;
                            finA0 = segChunks.front()->fitRes.a0;
                            finA2 = segChunks.front()->fitRes.a2;
                            ok = true;
                        }
                    } else if (rD >= segChunks.back()->centerRunIndex) {
                        if (segChunks.back()->success) {
                            finA1 = segChunks.back()->fitRes.a1;
                            finA0 = segChunks.back()->fitRes.a0;
                            finA2 = segChunks.back()->fitRes.a2;
                            ok = true;
                        }
                    } else {
                        for (size_t c = 0; c + 1 < segChunks.size(); ++c) {
                            double c1 = segChunks[c]->centerRunIndex;
                            double c2 = segChunks[c + 1]->centerRunIndex;
                            if (rD >= c1 && rD <= c2) {
                                double frac = (c2 > c1) ? ((rD - c1) / (c2 - c1)) : 0.0;
                                const auto &res1 = segChunks[c]->fitRes;
                                const auto &res2 = segChunks[c + 1]->fitRes;
                                if (segChunks[c]->success && segChunks[c + 1]->success) {
                                    finA1 = (1.0 - frac) * res1.a1 + frac * res2.a1;
                                    finA0 = (1.0 - frac) * res1.a0 + frac * res2.a0;
                                    finA2 = (1.0 - frac) * res1.a2 + frac * res2.a2;
                                    ok = true;
                                } else if (segChunks[c]->success) {
                                    finA1 = res1.a1; finA0 = res1.a0; finA2 = res1.a2; ok = true;
                                } else if (segChunks[c + 1]->success) {
                                    finA1 = res2.a1; finA0 = res2.a0; finA2 = res2.a2; ok = true;
                                }
                                break;
                            }
                        }
                    }
                } else if (parentChunk && parentChunk->success) {
                    finA1 = parentChunk->fitRes.a1;
                    finA0 = parentChunk->fitRes.a0;
                    finA2 = parentChunk->fitRes.a2;
                    ok = true;
                }

                if (ok) {
                    runRes.a0 = finA0;
                    runRes.a1 = finA1;
                    runRes.a2 = finA2;
                    runRes.status = RunCalibStatus::Success;
                    if (parentChunk && parentChunk->success) {
                        runRes.sRes = parentChunk->fitRes.sRes;
                        runRes.chi2NDF = parentChunk->fitRes.chi2NDF;
                        runRes.fittedAnchors = parentChunk->fitRes.fittedAnchors;
                    }
                    detSuccess++;
                    overallSuccessCount++;
                } else {
                    if (runRes.status == RunCalibStatus::Success) {
                        detSuccess++;
                        overallSuccessCount++;
                    } else {
                        detFail++;
                        overallFailureCount++;
                    }
                }

                traj.runResults[rIdx] = runRes;
                processedSteps++;
                const int overallPct = (processedSteps * 100) / std::max(1, totalSteps);
                emit runProcessed(detId, rIdx, nRuns, runRes);
                emit progressUpdated(overallPct, QString("Chunking Detector #%1: Run %2 of %3 (%4)")
                                                     .arg(detId).arg(rIdx + 1).arg(nRuns).arg(runRes.runFileName));
            }
        }

        traj.successCount = detSuccess;
        traj.failureCount = detFail;
        m_trajectories.push_back(traj);
        emit detectorFinished(detId, detSuccess, detFail);
    }

    const bool wasCancelled = m_stopRequested.load();
    emit batchFinished(wasCancelled, overallSuccessCount, overallFailureCount);
}
