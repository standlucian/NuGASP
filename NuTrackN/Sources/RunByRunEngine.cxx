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
        maxBin = std::clamp(static_cast<int>(std::round(fixed.fixedCentroid)), 1, totalBins - 2);
    } else {
        double maxVal = spectrum[lowCh];
        for (int ch = lowCh; ch <= highCh; ++ch) {
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

    // 2. Estimate empirical FWHM via half-maximum walk from maxBin
    double halfMax = baseVal + 0.5 * (maxVal - baseVal);
    double leftHM = static_cast<double>(lowCh);
    for (int ch = maxBin; ch >= lowCh; --ch) {
        if (spectrum[ch] <= halfMax) {
            double y1 = spectrum[ch];
            double y2 = (ch + 1 <= maxBin) ? spectrum[ch + 1] : halfMax;
            double frac = (std::abs(y2 - y1) > 1e-6) ? (halfMax - y1) / (y2 - y1) : 0.0;
            leftHM = ch + frac;
            break;
        }
    }
    double rightHM = static_cast<double>(highCh);
    for (int ch = maxBin; ch <= highCh; ++ch) {
        if (spectrum[ch] <= halfMax) {
            double y1 = (ch - 1 >= maxBin) ? spectrum[ch - 1] : halfMax;
            double y2 = spectrum[ch];
            double frac = (std::abs(y1 - y2) > 1e-6) ? (halfMax - y2) / (y1 - y2) : 0.0;
            rightHM = ch - frac;
            break;
        }
    }
    double obsFwhm = rightHM - leftHM;
    double estFwhm = (obsFwhm >= 0.8 && obsFwhm <= windowCh * 1.5)
        ? obsFwhm
        : (fixed.fixFwhm ? fixed.fixedFwhm : std::clamp(windowCh * 0.25, 1.5, 6.0));

    // 2. Dynamic peak extent & adaptive background windowing:
    // Look at how big the peak is (amplitude & FWHM) to set peak exclusion boundaries
    const double rawAmpl = std::max(1.0, maxVal - baseVal);
    const double estSigma = fixed.fixFwhm ? std::max(0.1, fixed.fixedFwhm / 2.35482) : std::max(0.2, estFwhm / 2.35482);
    const double nSig = std::clamp(std::sqrt(2.0 * std::log(std::max(10.0, rawAmpl))), 2.0, 3.5);
    const int peakHalfWidth = std::clamp(static_cast<int>(std::ceil(nSig * estSigma)), 2, std::max(2, static_cast<int>(windowCh * 0.8)));

    // Search outward from peak exclusion boundaries for true local background valleys (saddle points)
    // to avoid sampling inside peak wings or climbing adjacent neighbor lines
    int leftValley = std::max(0, maxBin - peakHalfWidth);
    const int lSearchMin = std::max(0, maxBin - static_cast<int>(windowCh));
    if (leftValley > lSearchMin) {
        for (int ch = leftValley; ch >= lSearchMin; --ch) {
            if (spectrum[ch] < spectrum[leftValley]) leftValley = ch;
        }
    }

    int rightValley = std::min(totalBins - 1, maxBin + peakHalfWidth);
    const int rSearchMax = std::min(totalBins - 1, maxBin + static_cast<int>(windowCh));
    if (rSearchMax > rightValley) {
        for (int ch = rightValley; ch <= rSearchMax; ++ch) {
            if (spectrum[ch] < spectrum[rightValley]) rightValley = ch;
        }
    }

    // Sample robust 3-bin medians at valley floors
    double leftBkg = 0.0;
    double rightBkg = 0.0;
    if (!fixed.fixBaseline) {
        std::vector<double> leftVals, rightVals;
        for (int c = std::max(0, leftValley - 1); c <= std::min(totalBins - 1, leftValley + 1); ++c) leftVals.push_back(spectrum[c]);
        for (int c = std::max(0, rightValley - 1); c <= std::min(totalBins - 1, rightValley + 1); ++c) rightVals.push_back(spectrum[c]);
        std::sort(leftVals.begin(), leftVals.end());
        std::sort(rightVals.begin(), rightVals.end());
        leftBkg = !leftVals.empty() ? leftVals[leftVals.size() / 2] : spectrum[leftValley];
        rightBkg = !rightVals.empty() ? rightVals[rightVals.size() / 2] : spectrum[rightValley];
        baseVal = (leftBkg + rightBkg) / 2.0;
    }

    // Physical Compton step capping: in physical HPGe detectors, Compton step is at most ~5% of peak amplitude
    const double rawStep = std::max(0.0, leftBkg - rightBkg);
    const double maxPhysStep = std::max(0.0, 0.05 * rawAmpl);
    const double estStep = std::min(rawStep, maxPhysStep);
    const bool useStep = fixed.fixStep ? (fixed.fixedStep > 0.0) : (!fixed.fixBaseline && estStep >= 1.5 && rawAmpl >= 25.0);

    // Adaptive fit window spanning from left valley to right valley
    int fitMin = std::max(1, std::max(maxBin - static_cast<int>(windowCh), leftValley));
    int fitMax = std::min(totalBins - 1, std::min(maxBin + static_cast<int>(windowCh), rightValley));
    int nFitBins = fitMax - fitMin + 1;
    if (nFitBins < 5) {
        fitMin = std::max(1, maxBin - peakHalfWidth - 2);
        fitMax = std::min(totalBins - 1, maxBin + peakHalfWidth + 2);
        nFitBins = fitMax - fitMin + 1;
    }
    if (nFitBins < 5) return result;

    // Use a lightweight ROOT TH1F for standard Gaussian + linear background + Compton step fit
    TH1F hTemp("hAnchorTemp", "Anchor", nFitBins, fitMin - 0.5, fitMax + 0.5);
    hTemp.SetDirectory(nullptr);
    for (int i = 0; i < nFitBins; ++i) {
        const int ch = fitMin + i;
        hTemp.SetBinContent(i + 1, spectrum[ch]);
        hTemp.SetBinError(i + 1, std::max(1.0, std::sqrt(std::max(0.0, spectrum[ch]))));
    }

    // Centered formula with Compton step:
    // [0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1]) + [5]*0.5*TMath::Erfc((x-[1])/(sqrt(2.0)*[2]))
    // Parameter 3 is the local high-energy baseline AT the centroid
    // Parameter 5 is the Compton step height
    const double bkgSlope = fixed.fixSlope ? fixed.fixedSlope : (useStep ? 0.0 : ((fitMax > fitMin) ? (rightBkg - leftBkg) / static_cast<double>(fitMax - fitMin) : 0.0));
    const double estAmpl = std::max(1.0, maxVal - (useStep ? (rightBkg > 0.0 ? rightBkg : baseVal) : baseVal));
    const double estMean = fixed.fixCentroid ? fixed.fixedCentroid : static_cast<double>(maxBin);

    TF1 fGaus("fAnchorGaus", "[0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1]) + [5]*0.5*TMath::Erfc((x-[1])/(sqrt(2.0)*[2]))", fitMin - 0.5, fitMax + 0.5);

    const double ampLow = 0.0;
    const double ampHigh = std::max(10.0, maxVal * 4.0);
    fGaus.SetParameter(0, std::clamp(estAmpl, ampLow, ampHigh));
    fGaus.SetParLimits(0, ampLow, ampHigh);

    if (fixed.fixCentroid) {
        fGaus.FixParameter(1, fixed.fixedCentroid);
    } else {
        const double meanLow = fitMin - 0.5;
        const double meanHigh = fitMax + 0.5;
        fGaus.SetParameter(1, std::clamp(estMean, meanLow, meanHigh));
        fGaus.SetParLimits(1, meanLow, meanHigh);
    }

    if (fixed.fixFwhm) {
        fGaus.FixParameter(2, std::max(0.1, fixed.fixedFwhm / 2.35482));
    } else {
        const double sigLow = 0.2;
        const double sigHigh = 25.0;
        fGaus.SetParameter(2, std::clamp(estSigma, sigLow, sigHigh));
        fGaus.SetParLimits(2, sigLow, sigHigh); // HPGe physical peak width boundaries
    }

    if (fixed.fixBaseline) {
        fGaus.FixParameter(3, fixed.fixedBaseline);
    } else {
        const double rawBkg = useStep ? (rightBkg > 0.0 ? rightBkg : baseVal) : baseVal;
        const double bkgLow = 0.0;
        const double bkgHigh = std::max(10.0, std::max(maxVal * 2.0, rawBkg * 2.0));
        fGaus.SetParameter(3, std::clamp(rawBkg, bkgLow, bkgHigh));
        fGaus.SetParLimits(3, bkgLow, bkgHigh);
    }

    if (fixed.fixSlope) {
        fGaus.FixParameter(4, fixed.fixedSlope);
    } else if (useStep) {
        // Critical: Lock slope to 0 when Compton step is active to eliminate collinearity and prevent sawtooth artifacts!
        fGaus.FixParameter(4, 0.0);
    } else {
        const double maxSlopeMag = std::max({ 2.0, maxVal * 0.05, std::abs(bkgSlope) * 1.5 });
        fGaus.SetParameter(4, std::clamp(bkgSlope, -maxSlopeMag, maxSlopeMag));
        fGaus.SetParLimits(4, -maxSlopeMag, maxSlopeMag);
    }

    if (fixed.fixStep) {
        fGaus.FixParameter(5, fixed.fixedStep);
    } else if (useStep) {
        const double stepLow = 0.0;
        const double stepHigh = std::max(2.0, maxPhysStep * 1.5 + 2.0);
        fGaus.SetParameter(5, std::clamp(estStep, stepLow, stepHigh));
        fGaus.SetParLimits(5, stepLow, stepHigh);
    } else {
        fGaus.FixParameter(5, 0.0);
    }

    // Fit with Poisson Likelihood + Bin Integration ("Q0NSIL"), fallback to standard Chi2 ("Q0NS")
    TFitResultPtr fitRes = hTemp.Fit(&fGaus, "Q0NSIL", "", fitMin - 0.5, fitMax + 0.5);
    if (!fitRes.Get() || (!fitRes->IsValid() && fitRes->Status() != 0 && !(fixed.fixCentroid && (fitRes->Status() % 1000 == 0 || fitRes->Status() == 4)))) {
        fitRes = hTemp.Fit(&fGaus, "Q0NS", "", fitMin - 0.5, fitMax + 0.5);
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
            // Parameter was fixed in ROOT MINUIT so GetParError(1) returns 0.
            // Compute realistic physical statistical centroid uncertainty from peak width and area:
            const double statErr = (area > 1.0) ? (fittedSigma / std::sqrt(area)) : 0.05;
            const double bkgVal = std::max(0.0, fGaus.GetParameter(3) + 0.5 * fGaus.GetParameter(5));
            const double bkgFactor = (area > 1.0 && bkgVal > 0.0)
                ? std::sqrt(1.0 + (2.0 * bkgVal * fwhm) / area)
                : 1.0;
            fittedMeanErr = std::clamp(statErr * bkgFactor, 0.03, 0.35);
        } else {
            fittedMeanErr = std::max(0.01, fGaus.GetParError(1));
        }

        if (fittedMean >= lowCh && fittedMean <= highCh && fwhm >= 0.3 && fwhm <= 50.0 && fittedAmpl > 0.0) {
            result.centroidCh = fittedMean;
            result.centroidErrCh = fittedMeanErr;
            result.fwhmCh = fwhm;
            result.amplitude = fittedAmpl;
            result.background = fGaus.GetParameter(3);
            result.bkgSlope = fGaus.GetParameter(4);
            result.bkgStep = fGaus.GetParameter(5);
            result.area = area;
            result.chi2 = chi2;
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
        result.background = fixed.fixBaseline ? fixed.fixedBaseline : (useStep && rightBkg > 0.0 ? rightBkg : baseVal);
        result.bkgSlope = fixed.fixSlope ? fixed.fixedSlope : (useStep ? 0.0 : bkgSlope);
        result.bkgStep = fixed.fixStep ? fixed.fixedStep : (useStep ? estStep : 0.0);
        result.area = estArea;
        result.chi2 = 1.0;
        result.isValid = true;
        return result;
    }

    // Fallback: Background-subtracted centroid & moment
    double sumW = 0.0, sumWX = 0.0, sumWXX = 0.0;
    const int momMin = std::max(1, maxBin - 6);
    const int momMax = std::min(totalBins - 1, maxBin + 6);
    const double momBase = (spectrum[momMin] + spectrum[momMax]) / 2.0;

    for (int ch = momMin; ch <= momMax; ++ch) {
        double w = std::max(0.0, spectrum[ch] - momBase);
        sumW += w;
        sumWX += w * ch;
        sumWXX += w * ch * ch;
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
        result.bkgSlope = 0.0;
        result.bkgStep = 0.0;
        result.area = sumW;
        result.chi2 = 1.0;
        result.isValid = (centroid >= lowCh && centroid <= highCh);
    }

    return result;
}

//==============================================================================
// computeCalibration
//==============================================================================
bool RunByRunEngine::computeCalibration(const std::vector<FittedAnchor> &anchors,
                                       bool useQuadratic,
                                       RunCalibResult &outResult,
                                       double maxAllowedChi2NDF)
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

    // Standard Linear Calibration: E(ch) = a0 + a1 * ch
    if (!useQuadratic || valid.size() < 3) {
        outResult.numCoeffs = 2;
        double sumW = 0.0, sumWX = 0.0, sumWY = 0.0, sumWXX = 0.0, sumWXY = 0.0;

        // Estimate rough scale factor (keV/ch) for dimensionally sound weighting
        double estGain = 1.0;
        if (valid.size() >= 2) {
            const double dE = std::abs(valid.back().energy - valid.front().energy);
            const double dCh = std::abs(valid.back().centroidCh - valid.front().centroidCh);
            if (dCh > 1e-3) estGain = dE / dCh;
        }
        if (estGain <= 1e-4 || std::isnan(estGain)) estGain = 1.0;

        for (const auto &a : valid) {
            const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
            const double sigTotSq = std::max(0.0025, (a.energyErr * a.energyErr) +
                                                    (estGain * estGain * centErr * centErr));
            const double w = 1.0 / sigTotSq;
            sumW += w;
            sumWX += w * a.centroidCh;
            sumWY += w * a.energy;
            sumWXX += w * a.centroidCh * a.centroidCh;
            sumWXY += w * a.centroidCh * a.energy;
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

        // Residual and Chi2 calculation (standard error propagation in energy space)
        double sumResSq = 0.0, sumChi2 = 0.0;
        for (const auto &a : valid) {
            const double fitE = outResult.a0 + outResult.a1 * a.centroidCh;
            const double res = a.energy - fitE;
            sumResSq += res * res;
            const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
            const double sigE = std::sqrt(std::max(0.0025, (a.energyErr * a.energyErr) +
                                                          (outResult.a1 * outResult.a1 * centErr * centErr)));
            sumChi2 += (res * res) / (sigE * sigE);
        }

        const int ndf = std::max(1, static_cast<int>(valid.size()) - 2);
        outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
        outResult.chi2NDF = sumChi2 / static_cast<double>(ndf);

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

    // Quadratic Calibration: E(ch) = a0 + a1 * ch + a2 * ch^2
    outResult.numCoeffs = 3;
    double m[3][3] = {{0.0}};
    double b[3] = {0.0};

    double estGain = 1.0;
    if (valid.size() >= 2) {
        const double dE = std::abs(valid.back().energy - valid.front().energy);
        const double dCh = std::abs(valid.back().centroidCh - valid.front().centroidCh);
        if (dCh > 1e-3) estGain = dE / dCh;
    }
    if (estGain <= 1e-4 || std::isnan(estGain)) estGain = 1.0;

    for (const auto &a : valid) {
        const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
        const double sigTotSq = std::max(0.0025, (a.energyErr * a.energyErr) +
                                                (estGain * estGain * centErr * centErr));
        const double w = 1.0 / sigTotSq;
        const double x = a.centroidCh;
        const double x2 = x * x;
        const double x3 = x2 * x;
        const double x4 = x2 * x2;
        const double y = a.energy;

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
        return computeCalibration(anchors, false, outResult);
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
        const double fitE = outResult.a0 + outResult.a1 * a.centroidCh + outResult.a2 * a.centroidCh * a.centroidCh;
        const double res = a.energy - fitE;
        sumResSq += res * res;
        const double centErr = (a.centroidErrCh > 0.0) ? a.centroidErrCh : 0.05;
        const double dEdx = std::abs(outResult.a1 + 2.0 * outResult.a2 * a.centroidCh);
        const double sigE = std::sqrt(std::max(0.0025, (a.energyErr * a.energyErr) +
                                                      (dEdx * dEdx * centErr * centErr)));
        sumChi2 += (res * res) / (sigE * sigE);
    }

    const int ndf = std::max(1, static_cast<int>(valid.size()) - 3);
    outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
    outResult.chi2NDF = sumChi2 / static_cast<double>(ndf);

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

//==============================================================================
// run: Detector-by-Detector Trajectory Tracking Loop
//==============================================================================
void RunByRunEngine::run()
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
        for (const auto &anchorDef : m_config.anchors) {
            double guessCh = (anchorDef.initialChannel > 0.0)
                ? anchorDef.initialChannel
                : (anchorDef.physicalEnergy / dispersion);
            baseCentroids.push_back(guessCh);
        }
        traj.lastValidCentroids = baseCentroids;

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

            // Fit each anchor peak within bounded search window around detector's moving centroid
            std::vector<FittedAnchor> fittedRunAnchors;

            for (size_t aIdx = 0; aIdx < m_config.anchors.size(); ++aIdx) {
                const auto &anchorDef = m_config.anchors[aIdx];
                const double centerCh = (aIdx < traj.lastValidCentroids.size())
                                            ? traj.lastValidCentroids[aIdx]
                                            : baseCentroids[aIdx];
                const double winCh = std::max(2.0, anchorDef.searchWindowCh);
                const double expFwhm = std::max(0.5, anchorDef.expectedFwhmCh);

                FittedAnchor fa = fitAnchorPeak(runSpectrum,
                                               centerCh,
                                               winCh,
                                               anchorDef.minCounts,
                                               anchorDef.physicalEnergy);
                fa.energyErr = anchorDef.energyError;

                // Fallback search around base centroid
                if (!fa.isValid && std::abs(centerCh - baseCentroids[aIdx]) > 0.5) {
                    fa = fitAnchorPeak(runSpectrum,
                                       baseCentroids[aIdx],
                                       winCh,
                                       anchorDef.minCounts,
                                       anchorDef.physicalEnergy);
                    fa.energyErr = anchorDef.energyError;
                }

                // Verify FWHM and drift from base centroid
                if (fa.isValid) {
                    if (expFwhm > 0.0 && fa.fwhmCh > expFwhm * 2.5) {
                        fa.isValid = false;
                    }
                    const double drift = std::abs(fa.centroidCh - baseCentroids[aIdx]);
                    if (drift > m_config.maxAllowedDriftCh) {
                        fa.isValid = false;
                    }
                }

                fittedRunAnchors.push_back(fa);
            }

            int validAnchorCount = 0;
            for (const auto &fa : fittedRunAnchors) {
                if (fa.isValid) validAnchorCount++;
            }
            const int minRequiredAnchors = m_config.useQuadratic ? 3 : 2;

            bool calibSuccess = false;
            if (validAnchorCount >= minRequiredAnchors) {
                calibSuccess = computeCalibration(fittedRunAnchors, m_config.useQuadratic, res, m_config.maxAllowedChi2NDF);
            } else {
                res.status = RunCalibStatus::NoPeakInWindow;
                res.failureReason = QString("Only %1 of %2 valid anchor peaks found (minimum %3 required).")
                                        .arg(validAnchorCount).arg(m_config.anchors.size()).arg(minRequiredAnchors);
            }

            if (calibSuccess) {
                res.status = RunCalibStatus::Success;
                res.isFallbackFromPrevious = false;
                // Update moving anchor centroids for all successfully fitted peaks
                for (size_t aIdx = 0; aIdx < fittedRunAnchors.size() && aIdx < traj.lastValidCentroids.size(); ++aIdx) {
                    if (fittedRunAnchors[aIdx].isValid) {
                        traj.lastValidCentroids[aIdx] = fittedRunAnchors[aIdx].centroidCh;
                    }
                }
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

        emit detectorFinished(detId, traj.successCount, traj.failureCount);
        m_trajectories.push_back(traj);
    }

    const bool wasCancelled = m_stopRequested.load();
    emit batchFinished(wasCancelled, overallSuccessCount, overallFailureCount);
}
