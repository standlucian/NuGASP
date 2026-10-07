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
FittedAnchor RunByRunEngine::fitAnchorPeak(const std::vector<double> &spectrum,
                                          double centerCh, double windowCh,
                                          double minCounts, double energy)
{
    FittedAnchor result;
    result.energy = energy;
    result.isValid = false;

    const int totalBins = static_cast<int>(spectrum.size());
    if (totalBins <= 10) return result;

    const int lowCh = std::max(1, static_cast<int>(std::floor(centerCh - windowCh)));
    const int highCh = std::min(totalBins - 2, static_cast<int>(std::ceil(centerCh + windowCh)));
    if (lowCh >= highCh) return result;

    // 1. Search for highest bin in search window
    int maxBin = lowCh;
    double maxVal = spectrum[lowCh];
    for (int ch = lowCh; ch <= highCh; ++ch) {
        if (spectrum[ch] > maxVal) {
            maxVal = spectrum[ch];
            maxBin = ch;
        }
    }

    // Baseline estimation from endpoints
    double baseVal = (spectrum[lowCh] + spectrum[highCh]) / 2.0;
    double netHeight = maxVal - baseVal;
    if (netHeight < minCounts || maxVal <= 0.0) {
        return result; // Low counts
    }

    // 2. Define local fitting window around detected maximum
    const int fitHalfWidth = std::clamp(static_cast<int>(windowCh * 0.75), 5, 25);
    const int fitMin = std::max(1, maxBin - fitHalfWidth);
    const int fitMax = std::min(totalBins - 1, maxBin + fitHalfWidth);
    const int nFitBins = fitMax - fitMin + 1;
    if (nFitBins < 5) return result;

    // Use a lightweight ROOT TH1F for standard Gaussian + linear background fit
    TH1F hTemp("hAnchorTemp", "Anchor", nFitBins, fitMin - 0.5, fitMax + 0.5);
    hTemp.SetDirectory(nullptr);
    for (int i = 0; i < nFitBins; ++i) {
        const int ch = fitMin + i;
        hTemp.SetBinContent(i + 1, spectrum[ch]);
        hTemp.SetBinError(i + 1, std::max(1.0, std::sqrt(std::max(0.0, spectrum[ch]))));
    }

    TF1 fGaus("fAnchorGaus", "gaus(0) + pol1(3)", fitMin - 0.5, fitMax + 0.5);
    const double bkgSlope = (spectrum[fitMax] - spectrum[fitMin]) / static_cast<double>(nFitBins);
    const double bkgConst = spectrum[fitMin];
    const double estAmpl = std::max(1.0, maxVal - baseVal);
    const double estMean = static_cast<double>(maxBin);
    const double estSigma = 2.5;

    fGaus.SetParameter(0, estAmpl);
    fGaus.SetParameter(1, estMean);
    fGaus.SetParameter(2, estSigma);
    fGaus.SetParameter(3, bkgConst);
    fGaus.SetParameter(4, bkgSlope);

    fGaus.SetParLimits(1, fitMin, fitMax);
    fGaus.SetParLimits(2, 0.4, 20.0); // HPGe physical peak width boundaries
    fGaus.SetParLimits(0, 0.0, maxVal * 3.0);

    TFitResultPtr fitRes = hTemp.Fit(&fGaus, "Q0NS", "", fitMin - 0.5, fitMax + 0.5);
    if (fitRes.Get() && fitRes->IsValid()) {
        const double fittedMean = fGaus.GetParameter(1);
        const double fittedMeanErr = fGaus.GetParError(1);
        const double fittedSigma = std::abs(fGaus.GetParameter(2));
        const double fittedAmpl = fGaus.GetParameter(0);
        const double fwhm = 2.35482 * fittedSigma;
        const double area = fittedAmpl * fittedSigma * std::sqrt(2.0 * M_PI);
        const double chi2 = fitRes->Chi2() / std::max(1, static_cast<int>(fitRes->Ndf()));

        if (fittedMean >= lowCh && fittedMean <= highCh && fwhm >= 0.8 && fwhm <= 40.0) {
            result.centroidCh = fittedMean;
            result.centroidErrCh = std::max(0.005, fittedMeanErr);
            result.fwhmCh = fwhm;
            result.area = area;
            result.chi2 = chi2;
            result.isValid = true;
            return result;
        }
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
                                       RunCalibResult &outResult)
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

        for (const auto &a : valid) {
            const double w = 1.0 / (a.centroidErrCh * a.centroidErrCh + 1e-4);
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

        // Residual calculation
        double sumResSq = 0.0, sumChi2 = 0.0;
        for (const auto &a : valid) {
            const double fitE = outResult.a0 + outResult.a1 * a.centroidCh;
            const double res = a.energy - fitE;
            sumResSq += res * res;
            const double sigE = std::max(0.05, std::abs(outResult.a1) * a.centroidErrCh);
            sumChi2 += (res * res) / (sigE * sigE);
        }

        const int ndf = std::max(1, static_cast<int>(valid.size()) - 2);
        outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
        outResult.chi2NDF = sumChi2 / static_cast<double>(ndf);
        outResult.status = RunCalibStatus::Success;
        outResult.failureReason.clear();
        return true;
    }

    // Quadratic Calibration: E(ch) = a0 + a1 * ch + a2 * ch^2
    outResult.numCoeffs = 3;
    double m[3][3] = {{0.0}};
    double b[3] = {0.0};

    for (const auto &a : valid) {
        const double w = 1.0 / (a.centroidErrCh * a.centroidErrCh + 1e-4);
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
        const double sigE = std::max(0.05, std::abs(outResult.a1) * a.centroidErrCh);
        sumChi2 += (res * res) / (sigE * sigE);
    }

    const int ndf = std::max(1, static_cast<int>(valid.size()) - 3);
    outResult.sRes = std::sqrt(sumResSq / static_cast<double>(ndf));
    outResult.chi2NDF = sumChi2 / static_cast<double>(ndf);
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

    const int refRunIdx = std::clamp(m_config.referenceRunIndex, 0, nRuns - 1);
    const QString refRunPath = m_config.runFilePaths[refRunIdx];

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

        // 1. Establish reference centroids on reference run for detector d
        std::vector<double> refSpectrum;
        QString err;
        std::vector<double> currentTrajectoryCentroids;

        bool refLoaded = ReadSpectrumData(refRunPath.toStdString(),
                                          m_config.spectrumFormat,
                                          m_config.spectrumChannels,
                                          detId,
                                          refSpectrum,
                                          &err);

        bool refAnchorsValid = false;
        if (refLoaded && !refSpectrum.empty()) {
            std::vector<FittedAnchor> refFitted;
            for (const auto &anchorDef : m_config.anchors) {
                double guessCh = anchorDef.initialChannel;
                if (guessCh <= 0.0) {
                    // Approximate initial channel from 1.0 keV/ch baseline if unprovided
                    guessCh = anchorDef.physicalEnergy;
                }
                FittedAnchor fa = fitAnchorPeak(refSpectrum, guessCh,
                                               anchorDef.searchWindowCh,
                                               anchorDef.minCounts,
                                               anchorDef.physicalEnergy);
                if (fa.isValid) {
                    currentTrajectoryCentroids.push_back(fa.centroidCh);
                    refFitted.push_back(fa);
                } else {
                    currentTrajectoryCentroids.push_back(guessCh);
                }
            }
            refAnchorsValid = (refFitted.size() >= 2);
        }

        traj.lastValidCentroids = currentTrajectoryCentroids;

        // 2. INNER LOOP: Run r in [0 .. nRuns - 1]
        for (int rIdx = 0; rIdx < nRuns; ++rIdx) {
            if (m_stopRequested.load()) break;

            const QString runPath = m_config.runFilePaths[rIdx];
            QFileInfo runInfo(runPath);

            // Extract numeric run number from filename (e.g. G0.0042 -> 42)
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

            if (!refAnchorsValid) {
                res.status = RunCalibStatus::RefAnchorFailed;
                res.failureReason = QString("Failed to establish baseline anchor peaks on Reference Run (%1)")
                                        .arg(QFileInfo(refRunPath).fileName());
                traj.runResults.push_back(res);
                traj.failureCount++;
                overallFailureCount++;
                processedSteps++;
                emit runProcessed(detId, rIdx, nRuns, res);
                continue;
            }

            // Load detector d spectrum for run r
            std::vector<double> runSpectrum;
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
            bool allAnchorsFound = true;
            QString missingAnchorDesc;

            for (size_t aIdx = 0; aIdx < m_config.anchors.size(); ++aIdx) {
                const auto &anchorDef = m_config.anchors[aIdx];
                const double centerCh = (aIdx < traj.lastValidCentroids.size())
                                            ? traj.lastValidCentroids[aIdx]
                                            : anchorDef.physicalEnergy;

                FittedAnchor fa = fitAnchorPeak(runSpectrum,
                                               centerCh,
                                               anchorDef.searchWindowCh,
                                               anchorDef.minCounts,
                                               anchorDef.physicalEnergy);

                // Verify drift from last valid centroid does not exceed max tolerance
                if (fa.isValid) {
                    const double drift = std::abs(fa.centroidCh - centerCh);
                    if (drift > m_config.maxAllowedDriftCh) {
                        fa.isValid = false;
                        allAnchorsFound = false;
                        missingAnchorDesc = QString("Drift on %1 keV peak (%2 ch) exceeded limit (%3 ch)")
                                                .arg(anchorDef.physicalEnergy, 0, 'f', 1)
                                                .arg(drift, 0, 'f', 1)
                                                .arg(m_config.maxAllowedDriftCh, 0, 'f', 1);
                        break;
                    }
                } else {
                    allAnchorsFound = false;
                    missingAnchorDesc = QString("Anchor peak %1 keV not found or counts too low")
                                            .arg(anchorDef.physicalEnergy, 0, 'f', 1);
                    break;
                }

                fittedRunAnchors.push_back(fa);
            }

            if (!allAnchorsFound) {
                res.status = RunCalibStatus::NoPeakInWindow;
                res.failureReason = missingAnchorDesc;
                traj.runResults.push_back(res);
                traj.failureCount++;
                overallFailureCount++;
            } else {
                // Compute calibration coefficients and update detector's trajectory
                if (computeCalibration(fittedRunAnchors, m_config.useQuadratic, res)) {
                    // Update moving anchor centroids for detector d
                    for (size_t aIdx = 0; aIdx < fittedRunAnchors.size() && aIdx < traj.lastValidCentroids.size(); ++aIdx) {
                        traj.lastValidCentroids[aIdx] = fittedRunAnchors[aIdx].centroidCh;
                    }
                    traj.runResults.push_back(res);
                    traj.successCount++;
                    overallSuccessCount++;
                } else {
                    traj.runResults.push_back(res);
                    traj.failureCount++;
                    overallFailureCount++;
                }
            }

            processedSteps++;
            const int overallPct = (processedSteps * 100) / std::max(1, totalSteps);
            emit runProcessed(detId, rIdx, nRuns, res);
            emit progressUpdated(overallPct, QString("Aligning Detector #%1: Run %2 of %3 (%4)")
                                                 .arg(detId).arg(rIdx + 1).arg(nRuns).arg(runInfo.fileName()));
        }

        emit detectorFinished(detId, traj.successCount, traj.failureCount);
        m_trajectories.push_back(traj);
    }

    const bool wasCancelled = m_stopRequested.load();
    emit batchFinished(wasCancelled, overallSuccessCount, overallFailureCount);
}
