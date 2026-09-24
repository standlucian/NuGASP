#include "PeakFit.h"
#include "canvas.h"
#include "Design.h"
#include "Integral.h"
#include "tracknhistogram.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

#include <QString>
#include <QtMath>
#include <QDialog>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPoint>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>
#include <QTimer>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QPolygon>

#include <TF1.h>
#include <TFormula.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TMatrixD.h>
#include <TMatrixDSym.h>
#include <TLine.h>
#include <TLatex.h>
#include <TSpectrum.h>
#include <TList.h>
#include <Math/MinimizerOptions.h>


//==============================================================================
// findMinValueInInterval
//==============================================================================
// Finds and returns the minimum bin content within the specified channel range.
// Clamps interval to [1, hist->GetNbinsX()].
//==============================================================================
Double_t findMinValueInInterval(TH1F *hist, int intervalStart, int intervalFinish)
{
    if (!hist) return 0.0;

    if (intervalStart > intervalFinish) {
        std::swap(intervalStart, intervalFinish);
    }
    const int nBins = hist->GetNbinsX();
    intervalStart  = std::max(1, std::min(nBins, intervalStart));
    intervalFinish = std::max(1, std::min(nBins, intervalFinish));

    Double_t minValue = hist->GetBinContent(intervalStart);
    for (int i = intervalStart + 1; i <= intervalFinish; ++i) {
        const Double_t val = hist->GetBinContent(i);
        if (val < minValue) {
            minValue = val;
        }
    }
    return minValue;
}

//==============================================================================
// findMaxValueInInterval
//==============================================================================
// Finds and returns the maximum bin content within the specified channel range.
// Clamps interval to [1, hist->GetNbinsX()].
//==============================================================================
Double_t findMaxValueInInterval(TH1F *hist, int intervalStart, int intervalFinish)
{
    if (!hist) return 0.0;

    if (intervalStart > intervalFinish) {
        std::swap(intervalStart, intervalFinish);
    }
    const int nBins = hist->GetNbinsX();
    intervalStart  = std::max(1, std::min(nBins, intervalStart));
    intervalFinish = std::max(1, std::min(nBins, intervalFinish));

    Double_t maxValue = hist->GetBinContent(intervalStart);
    for (int i = intervalStart + 1; i <= intervalFinish; ++i) {
        const Double_t val = hist->GetBinContent(i);
        if (val > maxValue) {
            maxValue = val;
        }
    }
    return maxValue;
}

//==============================================================================
// checkBackgrounds
//==============================================================================
// Validates background marker intervals:
// - Removes trailing unpaired marker if an odd count is provided.
// - Resolves overlapping regions by sorting markers into ordered pairs.
//==============================================================================
void checkBackgrounds(std::vector<Int_t> &background_markers)
{
    if (background_markers.empty()) {
        return;
    }

    // If an odd number of markers exists, remove the trailing unmatched marker
    if (background_markers.size() % 2 != 0) {
        const QString msg = QString("There is an odd number of background markers (%1), so the last one at %2 was removed.\n")
                                .arg(background_markers.size())
                                .arg(background_markers.back());
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        background_markers.pop_back();
    }

    // If background intervals overlap, sort them into non-overlapping pairs
    if (overlapping_markers(background_markers)) {
        std::cout << "The background markers produced overlapping regions.\n";
        CommandPrompt::getInstance()->appendPlainText("The background markers produced overlapping regions.\n");

        std::sort(background_markers.begin(), background_markers.end());

        std::cout << "Reordered into non-overlapping regions:\n";
        CommandPrompt::getInstance()->appendPlainText("Reordered into non-overlapping regions:\n");
        for (std::size_t i = 0; i < background_markers.size() / 2; ++i) {
            const QString region = QString("%1-%2\n")
                                       .arg(background_markers[2 * i])
                                       .arg(background_markers[2 * i + 1]);
            std::cout << region.toStdString();
            CommandPrompt::getInstance()->appendPlainText(region);
        }
    }
}

//==============================================================================
// checkRanges
//==============================================================================
// Validates that exactly two fit boundary markers are defined.
// If >2 markers are present, truncates to the first pair.
// Sorts the pair in ascending order.
//==============================================================================
bool checkRanges(std::vector<Double_t> &range_markers)
{
    if (range_markers.size() < 2) {
        const QString msg = QString("Fewer than 2 range markers defined (%1); fitting cannot proceed.\n")
                                .arg(range_markers.size());
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        return false;
    }

    if (range_markers.size() > 2) {
        const QString msg = QString("More than 2 range markers defined (%1). Using only the first pair: %2-%3.\n")
                                .arg(range_markers.size())
                                .arg(range_markers[0], 0, 'f', 0)
                                .arg(range_markers[1], 0, 'f', 0);
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        range_markers.resize(2);
    }

    std::sort(range_markers.begin(), range_markers.end());
    return true;
}

//==============================================================================
// checkGauss
//==============================================================================
// Validates that Gaussian centroid markers fall within [range_markers[0], range_markers[1]].
// Erases any out-of-bounds markers. Returns true if at least one centroid remains.
//==============================================================================
bool checkGauss(std::vector<Double_t> &gauss_markers, const std::vector<Double_t> &range_markers)
{
    if (range_markers.size() < 2) {
        return false;
    }

    const Double_t minRange = range_markers[0];
    const Double_t maxRange = range_markers[1];

    for (std::size_t i = 0; i < gauss_markers.size(); ) {
        if (gauss_markers[i] < minRange || gauss_markers[i] > maxRange) {
            const QString msg = QString("Peak centroid marker at %1 is outside designated fit range %2-%3 and has been removed.\n")
                                    .arg(gauss_markers[i], 0, 'f', 1)
                                    .arg(minRange, 0, 'f', 1)
                                    .arg(maxRange, 0, 'f', 1);
            std::cout << msg.toStdString();
            CommandPrompt::getInstance()->appendPlainText(msg);
            gauss_markers.erase(gauss_markers.begin() + i);
        } else {
            ++i;
        }
    }

    if (gauss_markers.empty()) {
        const QString msg = "No valid peak centroid markers remain within the fit range.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        return false;
    }

    return true;
}

//==============================================================================
// fitBackgroundHelper (Internal Helper)
//==============================================================================
// fitBackgroundHelper
//==============================================================================
// Accumulates selected background bins into a temporary histogram, fits,
// extracts covariance, computes background integral and error over the fit range,
// and draws the fitted baseline on the canvas.
//==============================================================================
void fitBackgroundHelper(QMainCanvas *mainCanvas)
{
    TH1F *activeHist = mainCanvas->HijF[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j];
    if (!activeHist || mainCanvas->background_markers.size() < 2) {
        return;
    }

    Double_t minimum = mainCanvas->maxValueInHistogram;

    // Analytical Least Squares Fit (avoids TH1::Fit artifacts with zero bins)
    Double_t S1 = 0.0, SX = 0.0, SXX = 0.0, SY = 0.0, SXY = 0.0;
    for (std::size_t i = 0; i < mainCanvas->background_markers.size() / 2; ++i) {
        const Int_t start  = mainCanvas->background_markers[2 * i];
        const Int_t finish = mainCanvas->background_markers[2 * i + 1];
        for (Int_t j = start; j <= finish; ++j) {
            Double_t xx = j; // Use exact bin
            Double_t yy = activeHist->GetBinContent(j);
            Double_t ei = 1.0; // In trackn.F: 1./ERR2(jj). We use uniform weighting for simplicity, or 1/max(yy,1)
            if (yy > 0) ei = 1.0 / yy; // Poisson weighting
            S1 += ei;
            SX += xx * ei;
            SXX += xx * xx * ei;
            SY += yy * ei;
            SXY += xx * yy * ei;
        }
    }

    Double_t deter = S1 * SXX - SX * SX;
    if (deter > 0.0) {
        mainCanvas->backgroundA0 = (SXX * SY - SX * SXY) / deter; // Intercept
        mainCanvas->backgroundA1 = (S1 * SXY - SX * SY) / deter;  // Slope
    } else {
        mainCanvas->backgroundA0 = minimum;
        mainCanvas->backgroundA1 = 0.0;
    }

    if (mainCanvas->backgroundFunction) {
        delete mainCanvas->backgroundFunction;
    }
    if (mainCanvas->background) {
        delete mainCanvas->background;
    }

    mainCanvas->background = new TFormula("background", "[0]*x+[1]");
    mainCanvas->backgroundFunction = new TF1("backgroundFunction", "background", 0, 10240);
    mainCanvas->backgroundFunction->SetParameter(0, mainCanvas->backgroundA1);
    mainCanvas->backgroundFunction->SetParameter(1, mainCanvas->backgroundA0);

    delete mainCanvas->backgroundCovarianceMatrix;
    mainCanvas->backgroundCovarianceMatrix = new TMatrixD(2, 2);
    if (deter > 0.0) {
        (*mainCanvas->backgroundCovarianceMatrix)(0,0) = S1 / deter; // Var(Slope)
        (*mainCanvas->backgroundCovarianceMatrix)(1,1) = SXX / deter; // Var(Intercept)
        (*mainCanvas->backgroundCovarianceMatrix)(0,1) = -SX / deter; // Cov
        (*mainCanvas->backgroundCovarianceMatrix)(1,0) = -SX / deter;
    }

    if (mainCanvas->range_markers.size() >= 2) {
        const Double_t r0 = mainCanvas->range_markers[0];
        const Double_t r1 = mainCanvas->range_markers[1];
        mainCanvas->backgroundIntegral = mainCanvas->backgroundFunction->Integral(r0, r1);
        
        Double_t params[2] = {mainCanvas->backgroundA1, mainCanvas->backgroundA0};
        mainCanvas->backgroundIntegralError = mainCanvas->backgroundFunction->IntegralError(
            r0, r1, params, mainCanvas->backgroundCovarianceMatrix->GetMatrixArray());
    } else {
        mainCanvas->backgroundIntegral = 0.0;
        mainCanvas->backgroundIntegralError = 0.0;
    }

    // Draw background line on canvas
    const Double_t xStart = mainCanvas->background_markers[0] - 0.5;
    const Double_t xEnd   = mainCanvas->background_markers.back() - 0.5;
    const Double_t yStart = mainCanvas->backgroundA1 * xStart + mainCanvas->backgroundA0;
    const Double_t yEnd   = mainCanvas->backgroundA1 * xEnd   + mainCanvas->backgroundA0;

    TLine *backgroundLine = new TLine(xStart, yStart, xEnd, yEnd);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);
    backgroundLine->Draw("same");
    mainCanvas->listOfObjectsDrawnOnScreen.Add(backgroundLine);

    mainCanvas->canvas->getCanvas()->Modified();
    mainCanvas->canvas->getCanvas()->Update();
}

//==============================================================================
// findPeakBoundariesWithSmoothing
//==============================================================================
// Uses a 3-point triangular smoothing probe:
//   S(i) = (N_{i-1} + 2*N_i + N_{i+1}) / 4.0
// to:
// 1. Refine the clicked position to the local smoothed apex.
// 2. Walk down the left and right slopes to locate the valley floors/minima
//    (separating the peak from neighboring peaks or flat baseline noise).
// 3. Add background padding bins, returning the optimal [xMin, xMax] fit interval.
// NOTE: Smoothing is used ONLY for boundary detection; the underlying histogram
// is NOT modified and raw counts are fitted.
//==============================================================================
void findPeakBoundariesWithSmoothing(TH1F *hist, int clickedBin, int &outApex, Double_t &outXMin, Double_t &outXMax)
{
    if (!hist) {
        outApex = clickedBin;
        outXMin = clickedBin - 20;
        outXMax = clickedBin + 20;
        return;
    }

    const int nBins = hist->GetNbinsX();
    clickedBin = std::max(1, std::min(nBins, clickedBin));

    // 3-point triangular smoothing filter probe
    auto getSmoothed = [&](int b) -> Double_t {
        int bPrev = std::max(1, b - 1);
        int bNext = std::min(nBins, b + 1);
        return (hist->GetBinContent(bPrev) + 2.0 * hist->GetBinContent(b) + hist->GetBinContent(bNext)) / 4.0;
    };

    // 1. Refine clicked bin to true local apex within +/- 8 bins
    int apex = clickedBin;
    Double_t maxSmoothed = getSmoothed(clickedBin);
    const int searchMin = std::max(1, clickedBin - 8);
    const int searchMax = std::min(nBins, clickedBin + 8);
    for (int b = searchMin; b <= searchMax; ++b) {
        Double_t sm = getSmoothed(b);
        if (sm > maxSmoothed) {
            maxSmoothed = sm;
            apex = b;
        }
    }
    outApex = apex;

    // 2. Walk down the left slope to find the left valley minimum
    int leftValley = apex;
    const int maxWalkLeft = std::max(1, apex - 50);
    int riseCountLeft = 0;
    for (int b = apex - 1; b >= maxWalkLeft; --b) {
        Double_t curr = getSmoothed(b);
        Double_t prev = getSmoothed(b + 1);
        // If counts start increasing as we step left, we crossed the valley minimum
        if (curr >= prev) {
            riseCountLeft++;
            if (riseCountLeft >= 2) {
                leftValley = b + 1;
                break;
            }
        } else {
            riseCountLeft = 0;
            leftValley = b;
        }
    }

    // 3. Walk down the right slope to find the right valley minimum
    int rightValley = apex;
    const int maxWalkRight = std::min(nBins, apex + 50);
    int riseCountRight = 0;
    for (int b = apex + 1; b <= maxWalkRight; ++b) {
        Double_t curr = getSmoothed(b);
        Double_t prev = getSmoothed(b - 1);
        // If counts start increasing as we step right, we crossed the valley minimum
        if (curr >= prev) {
            riseCountRight++;
            if (riseCountRight >= 2) {
                rightValley = b - 1;
                break;
            }
        } else {
            riseCountRight = 0;
            rightValley = b;
        }
    }

    // 4. Background padding: add 3 to 4 channels beyond the valleys to anchor the baseline
    int leftBoundary = std::max(1, leftValley - 3);
    int rightBoundary = std::min(nBins, rightValley + 3);

    // Ensure adequate minimum window width (at least 10 bins total)
    if (rightBoundary - leftBoundary < 10) {
        int needed = 10 - (rightBoundary - leftBoundary);
        int padLeft = needed / 2;
        int padRight = needed - padLeft;
        leftBoundary = std::max(1, leftBoundary - padLeft);
        rightBoundary = std::min(nBins, rightBoundary + padRight);
    }

    outXMin = static_cast<Double_t>(leftBoundary);
    outXMax = static_cast<Double_t>(rightBoundary);
}

//==============================================================================
// runAutoFit
//==============================================================================
// Performs an automated single-peak Gaussian fit around the clicked channel.
//
// Model function:
//   y(x) = [0] * exp( -0.5 * ((x - [1]) / [2])^2 ) + [3] * x + [4]
//
// Dynamically discovers the peak boundaries [fitMin, fitMax] via 3-point smoothing
// walkdown (valley detection) and fits raw data with Minuit.
// Outputs results to CommandPrompt console and registers centroid into puncte_calib2p.
//==============================================================================
void runAutoFit(QMainCanvas *mainCanvas, int x, int y)
{
    if (!mainCanvas) return;

    int binX = mainCanvas->getBinFromClick(x, y);
    TH1F *hist = mainCanvas->HijF[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j];
    if (!hist) return;

    // Use 3-point smoothing probe to find local apex and valley boundaries
    int apexBin = binX;
    Double_t fitMin = 0.0, fitMax = 0.0;
    findPeakBoundariesWithSmoothing(hist, binX, apexBin, fitMin, fitMax);

    const int iFitMin = static_cast<int>(fitMin);
    const int iFitMax = static_cast<int>(fitMax);

    // Estimate initial linear background from boundary flanks
    Double_t bkgLeft = 0.0;
    int nLeft = 0;
    for (int b = iFitMin; b <= std::min(iFitMin + 2, iFitMax); ++b) {
        bkgLeft += hist->GetBinContent(b);
        nLeft++;
    }
    bkgLeft = (nLeft > 0) ? (bkgLeft / nLeft) : hist->GetBinContent(iFitMin);

    Double_t bkgRight = 0.0;
    int nRight = 0;
    for (int b = std::max(iFitMin, iFitMax - 2); b <= iFitMax; ++b) {
        bkgRight += hist->GetBinContent(b);
        nRight++;
    }
    bkgRight = (nRight > 0) ? (bkgRight / nRight) : hist->GetBinContent(iFitMax);

    Double_t bkgSlope = (bkgRight - bkgLeft) / std::max(1.0, fitMax - fitMin);
    Double_t bkg0 = bkgLeft - bkgSlope * fitMin;

    Double_t apexCounts = hist->GetBinContent(apexBin);
    Double_t estBkgAtApex = bkgSlope * apexBin + bkg0;
    Double_t gaussianHeight = std::max(1.0, apexCounts - estBkgAtApex);
    Double_t gaussianSigma = std::max(1.0, (fitMax - fitMin) / 5.0);

    Double_t gaussianCenter = apexBin;
    Double_t gaussianFWHM = 0.0;
    Double_t gaussianCenterError = 0.0, gaussianIntegral = 0.0;
    Double_t gaussianIntegralError = 0.0, gaussianFWHMError = 0.0;

    // Standard Gaussian + Linear Background model:
    // [0] = Amplitude, [1] = Centroid, [2] = Sigma, [3] = Background Slope, [4] = Background Intercept
    mainCanvas->gaussianWithBackground = new TFormula(
        "gaussianWithBackground", "[0]*exp(-0.5*((x-[1])/[2])^2)+[3]*x+[4]");
    mainCanvas->gaussianWithBackgroundFunction = new TF1(
        "gaussianWithBackgroundFunction", "gaussianWithBackground", fitMin, fitMax);

    mainCanvas->background = new TFormula("background", "[0]*x+[1]");
    mainCanvas->backgroundFunction = new TF1("backgroundFunction", "background", fitMin, fitMax);

    mainCanvas->gaussianCenterMarkerText = new TLatex();

    mainCanvas->m_lastFitType = 1;
    mainCanvas->m_lastAutoFitX = x;
    mainCanvas->m_lastAutoFitY = y;

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    const bool isCalib = (trackHist && trackHist->IsCalibrated());

    if (!mainCanvas->m_isRefitting) {
        mainCanvas->m_bkgFixed = false;
        mainCanvas->m_peakFixedStates.assign(1, PeakParamState{});
    }

    const bool fixCent0 = (!mainCanvas->m_peakFixedStates.empty() && mainCanvas->m_peakFixedStates[0].fixCentroid);
    const bool fixAmp0  = (!mainCanvas->m_peakFixedStates.empty() && mainCanvas->m_peakFixedStates[0].fixAmp);
    const bool fixW0    = (!mainCanvas->m_peakFixedStates.empty() && mainCanvas->m_peakFixedStates[0].fixWidth);

    // Initial parameter estimates / fixed constraints
    if (fixAmp0) {
        mainCanvas->gaussianWithBackgroundFunction->FixParameter(0, mainCanvas->m_peakFixedStates[0].ampVal);
    } else {
        mainCanvas->gaussianWithBackgroundFunction->SetParameter(0, gaussianHeight);
        mainCanvas->gaussianWithBackgroundFunction->SetParLimits(0, 0.0, hist->GetMaximum() * 2.0);
    }

    if (fixCent0) {
        double fixedCenterCh = mainCanvas->m_peakFixedStates[0].centroidVal;
        if (isCalib) {
            fixedCenterCh = trackHist->EnergyToChannel(fixedCenterCh);
        }
        mainCanvas->gaussianWithBackgroundFunction->FixParameter(1, fixedCenterCh);
    } else {
        mainCanvas->gaussianWithBackgroundFunction->SetParameter(1, gaussianCenter);
        mainCanvas->gaussianWithBackgroundFunction->SetParLimits(1, fitMin, fitMax);
    }

    if (fixW0) {
        const double fixedW = mainCanvas->m_peakFixedStates[0].widthVal;
        double fixedSigmaCh = 0.0;
        if (isCalib) {
            double cE = trackHist->ChannelToEnergy(gaussianCenter);
            if (fixCent0) {
                cE = mainCanvas->m_peakFixedStates[0].centroidVal;
            }
            const Double_t ch1 = trackHist->EnergyToChannel(cE - fixedW * 0.5);
            const Double_t ch2 = trackHist->EnergyToChannel(cE + fixedW * 0.5);
            fixedSigmaCh = std::abs(ch2 - ch1) / 2.35482;
        } else {
            fixedSigmaCh = fixedW / 2.35482;
        }
        mainCanvas->gaussianWithBackgroundFunction->FixParameter(2, fixedSigmaCh);
    } else {
        mainCanvas->gaussianWithBackgroundFunction->SetParameter(2, gaussianSigma);
        mainCanvas->gaussianWithBackgroundFunction->SetParLimits(2, 0.3, (fitMax - fitMin));
    }

    if (mainCanvas->m_bkgFixed) {
        bkgSlope = mainCanvas->m_bkgSlopeVal;
        bkg0     = mainCanvas->m_bkgInterceptVal;
        mainCanvas->gaussianWithBackgroundFunction->FixParameter(3, bkgSlope);
        mainCanvas->gaussianWithBackgroundFunction->FixParameter(4, bkg0);
    } else {
        mainCanvas->gaussianWithBackgroundFunction->SetParameter(3, bkgSlope);
        mainCanvas->gaussianWithBackgroundFunction->SetParameter(4, bkg0);
    }

    // Fit with Minuit on the raw, unsmoothed histogram
    TFitResultPtr fitResult = hist->Fit(mainCanvas->gaussianWithBackgroundFunction, "QMRS", "same");

    bkgSlope = mainCanvas->gaussianWithBackgroundFunction->GetParameter(3);
    bkg0     = mainCanvas->gaussianWithBackgroundFunction->GetParameter(4);
    mainCanvas->backgroundA1 = bkgSlope;
    mainCanvas->backgroundA0 = bkg0;
    if (!mainCanvas->m_bkgFixed) {
        mainCanvas->m_bkgSlopeVal = bkgSlope;
        mainCanvas->m_bkgInterceptVal = bkg0;
    }

    mainCanvas->backgroundFunction->SetRange(fitMin, fitMax);
    mainCanvas->backgroundFunction->FixParameter(0, bkgSlope);
    mainCanvas->backgroundFunction->FixParameter(1, bkg0);

    gaussianSigma = std::abs(mainCanvas->gaussianWithBackgroundFunction->GetParameter(2));
    gaussianFWHM  = gaussianSigma * 2.35482;
    gaussianIntegral = mainCanvas->gaussianWithBackgroundFunction->Integral(fitMin, fitMax)
                     - mainCanvas->backgroundFunction->Integral(fitMin, fitMax);
    if (fitResult.Get()) {
        gaussianIntegralError = mainCanvas->gaussianWithBackgroundFunction->IntegralError(
            fitMin, fitMax, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
    }

    // Adaptive refit if fitted peak is broader than the detected boundaries
    if (gaussianFWHM * 2.5 > (fitMax - fitMin)) {
        fitMin = std::max(1.0, gaussianCenter - gaussianFWHM * 3.0);
        fitMax = std::min(static_cast<Double_t>(hist->GetNbinsX()), gaussianCenter + gaussianFWHM * 3.0);
        mainCanvas->gaussianWithBackgroundFunction->SetRange(fitMin, fitMax);
        mainCanvas->backgroundFunction->SetRange(fitMin, fitMax);

        if (mainCanvas->m_bkgFixed) {
            mainCanvas->gaussianWithBackgroundFunction->FixParameter(3, bkgSlope);
            mainCanvas->gaussianWithBackgroundFunction->FixParameter(4, bkg0);
        }
        if (fixAmp0) {
            mainCanvas->gaussianWithBackgroundFunction->FixParameter(0, mainCanvas->m_peakFixedStates[0].ampVal);
        }
        if (fixCent0) {
            double fixedCenterCh = mainCanvas->m_peakFixedStates[0].centroidVal;
            if (isCalib) {
                fixedCenterCh = trackHist->EnergyToChannel(fixedCenterCh);
            }
            mainCanvas->gaussianWithBackgroundFunction->FixParameter(1, fixedCenterCh);
        }
        if (fixW0) {
            const double fixedW = mainCanvas->m_peakFixedStates[0].widthVal;
            double fixedSigmaCh = 0.0;
            if (isCalib) {
                double cE = trackHist->ChannelToEnergy(gaussianCenter);
                if (fixCent0) {
                    cE = mainCanvas->m_peakFixedStates[0].centroidVal;
                }
                const Double_t ch1 = trackHist->EnergyToChannel(cE - fixedW * 0.5);
                const Double_t ch2 = trackHist->EnergyToChannel(cE + fixedW * 0.5);
                fixedSigmaCh = std::abs(ch2 - ch1) / 2.35482;
            } else {
                fixedSigmaCh = fixedW / 2.35482;
            }
            mainCanvas->gaussianWithBackgroundFunction->FixParameter(2, fixedSigmaCh);
        }

        fitResult = hist->Fit(mainCanvas->gaussianWithBackgroundFunction, "QMRS", "");

        gaussianSigma = std::abs(mainCanvas->gaussianWithBackgroundFunction->GetParameter(2));
        gaussianFWHM  = gaussianSigma * 2.35482;
        gaussianIntegral = mainCanvas->gaussianWithBackgroundFunction->Integral(fitMin, fitMax)
                         - mainCanvas->backgroundFunction->Integral(fitMin, fitMax);
        if (fitResult.Get()) {
            gaussianIntegralError = mainCanvas->gaussianWithBackgroundFunction->IntegralError(
                fitMin, fitMax, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
        }

        bkgSlope = mainCanvas->gaussianWithBackgroundFunction->GetParameter(3);
        bkg0     = mainCanvas->gaussianWithBackgroundFunction->GetParameter(4);
        mainCanvas->backgroundA1 = bkgSlope;
        mainCanvas->backgroundA0 = bkg0;
        if (!mainCanvas->m_bkgFixed) {
            mainCanvas->m_bkgSlopeVal = bkgSlope;
            mainCanvas->m_bkgInterceptVal = bkg0;
        }
    }

    gaussianHeight      = mainCanvas->gaussianWithBackgroundFunction->GetParameter(0);
    gaussianCenter      = mainCanvas->gaussianWithBackgroundFunction->GetParameter(1);
    gaussianCenterError = mainCanvas->gaussianWithBackgroundFunction->GetParError(1);
    gaussianFWHMError   = mainCanvas->gaussianWithBackgroundFunction->GetParError(2) * 2.35482;

    // Check if active histogram is calibrated to display physical energy (keV)
    Double_t dispEnergy = gaussianCenter;
    Double_t dispEnergyError = gaussianCenterError;
    Double_t dispFWHM = gaussianFWHM;
    Double_t dispFWHMError = gaussianFWHMError;
    if (isCalib) {
        dispEnergy = trackHist->ChannelToEnergy(gaussianCenter);
        dispEnergyError = std::abs(trackHist->ChannelToEnergy(gaussianCenter + gaussianCenterError) - dispEnergy);

        dispFWHM = std::abs(trackHist->ChannelToEnergy(gaussianCenter + gaussianFWHM * 0.5) -
                            trackHist->ChannelToEnergy(gaussianCenter - gaussianFWHM * 0.5));
        dispFWHMError = (gaussianFWHM > 0.0) ? (gaussianFWHMError * dispFWHM / gaussianFWHM) : 0.0;
    }

    mainCanvas->gaussCenters[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(gaussianCenter);
    mainCanvas->gaussCentersHeight[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(gaussianHeight);

    // Draw fit curves
    mainCanvas->canvas->getCanvas()->cd(
        (mainCanvas->SelectedElement_i - 1) * mainCanvas->maxElement_j + mainCanvas->SelectedElement_j);
    mainCanvas->gaussianWithBackgroundFunction->Draw("same");

    mainCanvas->backgroundFunction->SetLineColor(kBlue);
    mainCanvas->backgroundFunction->Draw("same");

    mainCanvas->autoFitMarkers[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(
        mainCanvas->backgroundFunction);
    mainCanvas->autoFitMarkers[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(
        mainCanvas->gaussianWithBackgroundFunction);

    if (mainCanvas->m_isRefitting && !mainCanvas->puncte_calib2p.empty()) {
        mainCanvas->puncte_calib2p.back() = gaussianCenter;
    } else {
        mainCanvas->puncte_calib2p.push_back(gaussianCenter);
    }
    mainCanvas->renderPeakLabels(mainCanvas->SelectedElement_i, mainCanvas->SelectedElement_j);

    // Calculate fit quality metrics: RMS residual and Mean Relative Residual (%)
    Double_t sumSqDiff = 0.0;
    Double_t sumRelDiff = 0.0;
    Int_t nBinsFit = 0;
    const Int_t bMin = std::max(1, static_cast<int>(std::round(fitMin)));
    const Int_t bMax = std::min(hist->GetNbinsX(), static_cast<int>(std::round(fitMax)));
    for (Int_t b = bMin; b <= bMax; ++b) {
        Double_t x = hist->GetBinCenter(b);
        Double_t y = hist->GetBinContent(b);
        Double_t fy = mainCanvas->gaussianWithBackgroundFunction->Eval(x);
        Double_t diff = y - fy;
        sumSqDiff += diff * diff;
        if (y > 0.0) {
            sumRelDiff += std::abs(diff) / y;
        }
        nBinsFit++;
    }
    Double_t rmsResidual = (nBinsFit > 0) ? std::sqrt(sumSqDiff / nBinsFit) : 0.0;
    Double_t meanRelResidualPct = (nBinsFit > 0) ? (sumRelDiff / nBinsFit * 100.0) : 0.0;

    // Report table output: if calibrated, show ONLY Energy; if uncalibrated, show ONLY Channels
    QString numberStr           = QString::number(1).leftJustified(10, ' ');
    QString gaussianIntegralStr = QString("%1(%2)").arg(gaussianIntegral, 0, 'f', 0).arg(qRound(gaussianIntegralError));

    if (isCalib) {
        QString headerRow = QString("%1%2%3%4")
            .arg("Peak#",  -10, QChar(' '))
            .arg("Energy", -18, QChar(' '))
            .arg("Area",   -25, QChar(' '))
            .arg("Width",  -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(headerRow);

        std::cout << std::left
                  << std::setw(10) << "Peak#"
                  << std::setw(18) << "Energy"
                  << std::setw(25) << "Area"
                  << std::setw(15) << "Width" << std::endl;

        QString energyStr   = QString("%1(%2)").arg(dispEnergy, 0, 'f', 2).arg(qCeil(dispEnergyError * 100));
        QString fwhmStr     = QString("%1(%2)").arg(dispFWHM, 0, 'f', 2).arg(qCeil(dispFWHMError * 100));

        QString dataRow = QString("%1%2%3%4")
            .arg(numberStr)
            .arg(energyStr,           -18, QChar(' '))
            .arg(gaussianIntegralStr, -25, QChar(' '))
            .arg(fwhmStr,             -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(dataRow + "\n");

        std::cout << std::left
                  << std::setw(10) << "1"
                  << std::setw(18) << energyStr.toStdString()
                  << std::setw(25) << gaussianIntegralStr.toStdString()
                  << std::setw(15) << fwhmStr.toStdString() << std::endl;
    } else {
        QString headerRow = QString("%1%2%3%4")
            .arg("Peak#",   -10, QChar(' '))
            .arg("Channel", -18, QChar(' '))
            .arg("Area",    -25, QChar(' '))
            .arg("Width",   -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(headerRow);

        std::cout << std::left
                  << std::setw(10) << "Peak#"
                  << std::setw(18) << "Channel"
                  << std::setw(25) << "Area"
                  << std::setw(15) << "Width" << std::endl;

        QString centerStr   = QString("%1(%2)").arg(gaussianCenter, 0, 'f', 2).arg(qCeil(gaussianCenterError * 100));
        QString fwhmStr     = QString("%1(%2)").arg(dispFWHM, 0, 'f', 2).arg(qCeil(dispFWHMError * 100));

        QString dataRow = QString("%1%2%3%4")
            .arg(numberStr)
            .arg(centerStr,           -18, QChar(' '))
            .arg(gaussianIntegralStr, -25, QChar(' '))
            .arg(fwhmStr,             -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(dataRow + "\n");

        std::cout << std::left
                  << std::setw(10) << "1"
                  << std::setw(18) << centerStr.toStdString()
                  << std::setw(25) << gaussianIntegralStr.toStdString()
                  << std::setw(15) << fwhmStr.toStdString() << std::endl;
    }

    // Show unfocused dialog with parameters in top right corner
    Double_t totalChi2 = fitResult.Get() ? fitResult->Chi2() : mainCanvas->gaussianWithBackgroundFunction->GetChisquare();
    Int_t ndf = fitResult.Get() ? fitResult->Ndf() : mainCanvas->gaussianWithBackgroundFunction->GetNDF();
    Double_t redChi2 = (ndf > 0) ? (totalChi2 / ndf) : 0.0;

    QString redChi2Str = (redChi2 >= 1e4) ? QString::number(redChi2, 'g', 4) : QString::number(redChi2, 'f', 2);

    QString autoHtml = "<div style='color:#abb2bf; font-family: monospace; font-size:11px;'>";
    autoHtml += "<div style='margin-bottom:2px;'>";
    autoHtml += "<span style='color:#61afef; font-weight:bold;'>&#9654; Fit Quality</span><br>";
    autoHtml += QString("&nbsp;&bull; Reduced &chi;&sup2; (&chi;&sup2;/NDF): <b style='color:#98c379;'>%1</b><br>").arg(redChi2Str);
    autoHtml += QString("&nbsp;&bull; Mean Residual: <b style='color:#61afef;'>%1%</b> (RMS: %2 cts)<br>")
                    .arg(meanRelResidualPct, 0, 'f', 1).arg(rmsResidual, 0, 'f', 0);
    autoHtml += "</div>";
    autoHtml += "</div>";

    std::vector<FittedPeakData> peaks(1);
    peaks[0].peakIndex = 0;
    peaks[0].centroid = dispEnergy;
    peaks[0].centroidErr = dispEnergyError;
    peaks[0].amplitude = gaussianHeight;
    peaks[0].amplitudeErr = mainCanvas->gaussianWithBackgroundFunction->GetParError(0);
    peaks[0].width = dispFWHM;
    peaks[0].widthErr = dispFWHMError;
    peaks[0].netArea = gaussianIntegral;
    peaks[0].netAreaErr = gaussianIntegralError;
    peaks[0].isCalibrated = isCalib;

    showFitParametersDialog(mainCanvas, "AutoFit Parameters", autoHtml, peaks);

    if (mainCanvas->m_isAreaLoggingEnabled) {
        mainCanvas->writeAreaLogHeader(true);
        double bkgVal = mainCanvas->backgroundFunction ? mainCanvas->backgroundFunction->Integral(fitMin, fitMax) : 0.0;
        double grossVal = gaussianIntegral + bkgVal;
        mainCanvas->writeAreaLogData(dispEnergy, dispFWHM, grossVal, gaussianIntegral, bkgVal, gaussianIntegralError);
    }

    TList *funcList = hist->GetListOfFunctions();
    if (funcList) {
        TObject *fitFunc = funcList->FindObject(mainCanvas->gaussianWithBackgroundFunction->GetName());
        if (fitFunc) {
            funcList->Remove(fitFunc);
        }
    }

    mainCanvas->canvas->getCanvas()->Modified();
    mainCanvas->canvas->getCanvas()->Update();
}

//==============================================================================
//==============================================================================
// runMultiPeakFit
//==============================================================================
// Performs multi-peak Gaussian fitting with background over the designated range,
// adhering to legacy GASP / XTrackN standards:
//
// 1. Validates and sanitizes background, range, and Gaussian centroid markers.
// 2. If background markers ('B') are present (>=2), pre-fits linear background
//    over background intervals and fixes parameters (like GASP when IFBGD=1).
//    If no background markers are present, estimates baseline from range boundaries
//    and leaves background parameters FREE to fit (like GASP when IFBGD=0).
// 3. Links all Gaussian peaks in the range to a single COMMON width parameter
//    (sigma, par[2]), replicating the GASP GFFUN formulation.
// 4. Assembles composite function:
//      F(x) = BkgSlope*x + BkgConst + sum_i( Amp_i * exp( -0.5 * ((x - Mean_i)/Sigma)^2 ) )
// 5. Fits with Minuit (Q M R S), with adaptive strategy/tolerance retries if needed.
// 6. Post-fit: extracts individual peak integrals, sub-covariance matrices,
//    and combines peak error with background uncertainty:
//      sigma_total = sqrt( sigma_fit^2 + sigma_bkg^2 )
// 7. Outputs monospace analysis table to CommandPrompt and stdout, and renders peak badges.
//==============================================================================
void runMultiPeakFit(QMainCanvas *mainCanvas)
{
    if (!mainCanvas) return;

    mainCanvas->m_lastFitType = 2;

    mainCanvas->IdentifyLastClickedHistogram(mainCanvas->mousePilgrimX, mainCanvas->mousePilgrimY);

    checkBackgrounds(mainCanvas->background_markers);

    if (!checkRanges(mainCanvas->range_markers)) {
        return;
    }

    if (!checkGauss(mainCanvas->gauss_markers, mainCanvas->range_markers)) {
        return;
    }

    TH1F *activeHist = mainCanvas->HijF[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j];
    if (!activeHist) return;

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(activeHist);
    const bool isCalib = (trackHist && trackHist->IsCalibrated());

    const Double_t r0 = mainCanvas->range_markers[0];
    const Double_t r1 = mainCanvas->range_markers[1];
    const Double_t maxValue = findMaxValueInInterval(activeHist, r0, r1);
    const std::size_t nPeaks = mainCanvas->gauss_markers.size();

    if (!mainCanvas->m_isRefitting) {
        mainCanvas->m_bkgFixed = false;
        mainCanvas->m_peakFixedStates.assign(nPeaks, PeakParamState{});
    }

    // Check whether explicit background intervals were marked ('B' markers)
    const bool hasExplicitBkg = (mainCanvas->background_markers.size() >= 2);

    if (mainCanvas->m_bkgFixed) {
        mainCanvas->backgroundA1 = mainCanvas->m_bkgSlopeVal;
        mainCanvas->backgroundA0 = mainCanvas->m_bkgInterceptVal;
    } else if (hasExplicitBkg) {
        // Fit linear background over marked background regions and draw baseline
        fitBackgroundHelper(mainCanvas);
        mainCanvas->m_bkgSlopeVal = mainCanvas->backgroundA1;
        mainCanvas->m_bkgInterceptVal = mainCanvas->backgroundA0;
    } else {
        // GASP behavior when no background markers are set:
        // Estimate initial linear baseline connecting range boundary channels
        const int nBinsX = activeHist->GetNbinsX();
        const Int_t bin0 = std::max(1, std::min(nBinsX, static_cast<int>(std::round(r0))));
        const Int_t bin1 = std::max(1, std::min(nBinsX, static_cast<int>(std::round(r1))));
        const Double_t y0 = activeHist->GetBinContent(bin0);
        const Double_t y1 = activeHist->GetBinContent(bin1);
        const Double_t dx = (r1 != r0) ? (r1 - r0) : 1.0;
        const Double_t estSlope = (y1 - y0) / dx;
        const Double_t estIntercept = y0 - estSlope * r0;

        mainCanvas->backgroundA0 = estIntercept;
        mainCanvas->backgroundA1 = estSlope;
        mainCanvas->backgroundIntegral = 0.5 * (y0 + y1) * std::abs(r1 - r0);
        mainCanvas->backgroundIntegralError = 0.0;
        mainCanvas->m_bkgSlopeVal = mainCanvas->backgroundA1;
        mainCanvas->m_bkgInterceptVal = mainCanvas->backgroundA0;
    }

    const bool decoupleWidths = mainCanvas->m_uncoupleWidths;

    // Construct composite function:
    // Par 0: Background slope
    // Par 1: Background intercept
    // If NOT decoupled:
    //   Par 2: Common Gaussian width (Sigma) - shared by all peaks (GASP GFFUN model)
    //   Par 3 + 2*i: Amplitude of peak i
    //   Par 4 + 2*i: Centroid of peak i
    // If decoupled:
    //   Par 2 + 3*i: Amplitude of peak i
    //   Par 3 + 3*i: Centroid of peak i
    //   Par 4 + 3*i: Individual Gaussian width (Sigma) of peak i
    std::string formulaStr = "[0]*x + [1]";
    if (!decoupleWidths) {
        for (std::size_t i = 0; i < nPeaks; ++i) {
            const int ampIdx  = 3 + 2 * static_cast<int>(i);
            const int meanIdx = 4 + 2 * static_cast<int>(i);
            formulaStr += " + [" + std::to_string(ampIdx) + "]*exp(-0.5*((x-["
                        + std::to_string(meanIdx) + "])/[2])^2)";
        }
    } else {
        for (std::size_t i = 0; i < nPeaks; ++i) {
            const int ampIdx  = 2 + 3 * static_cast<int>(i);
            const int meanIdx = 3 + 3 * static_cast<int>(i);
            const int sigIdx  = 4 + 3 * static_cast<int>(i);
            formulaStr += " + [" + std::to_string(ampIdx) + "]*exp(-0.5*((x-["
                        + std::to_string(meanIdx) + "])/[" + std::to_string(sigIdx) + "])^2)";
        }
    }

    TF1 *fullFunction = new TF1("fullFunction", formulaStr.c_str(), r0, r1);
    fullFunction->SetParName(0, "BkgSlope");
    fullFunction->SetParName(1, "BkgConst");

    if (!decoupleWidths) {
        fullFunction->SetParName(2, "Sigma");
        bool isCommonWFixed = false;
        double commonWVal = 0.0;
        for (const auto &ps : mainCanvas->m_peakFixedStates) {
            if (ps.fixWidth) {
                isCommonWFixed = true;
                commonWVal = ps.widthVal;
                break;
            }
        }
        if (isCommonWFixed) {
            double fixedSigmaCh = 0.0;
            if (isCalib) {
                const Double_t midCenter = 0.5 * (r0 + r1);
                const Double_t midE = trackHist->ChannelToEnergy(midCenter);
                const Double_t ch1 = trackHist->EnergyToChannel(midE - commonWVal * 0.5);
                const Double_t ch2 = trackHist->EnergyToChannel(midE + commonWVal * 0.5);
                fixedSigmaCh = std::abs(ch2 - ch1) / 2.35482;
            } else {
                fixedSigmaCh = commonWVal / 2.35482;
            }
            fullFunction->FixParameter(2, fixedSigmaCh);
        } else {
            fullFunction->SetParameter(2, 3.0);
            fullFunction->SetParLimits(2, 0.4, std::abs(r1 - r0) * 2.0);
        }
        for (std::size_t i = 0; i < nPeaks; ++i) {
            fullFunction->SetParName(3 + 2 * i, Form("Amp%zu", i + 1));
            fullFunction->SetParName(4 + 2 * i, Form("Mean%zu", i + 1));
        }
    } else {
        for (std::size_t i = 0; i < nPeaks; ++i) {
            fullFunction->SetParName(2 + 3 * i, Form("Amp%zu", i + 1));
            fullFunction->SetParName(3 + 3 * i, Form("Mean%zu", i + 1));
            fullFunction->SetParName(4 + 3 * i, Form("Sigma%zu", i + 1));
        }
    }

    if (mainCanvas->m_bkgFixed) {
        fullFunction->FixParameter(0, mainCanvas->backgroundA1);
        fullFunction->FixParameter(1, mainCanvas->backgroundA0);
    } else if (hasExplicitBkg) {
        // Fix background parameters from pre-fitted background line
        fullFunction->FixParameter(0, mainCanvas->backgroundA1);
        fullFunction->FixParameter(1, mainCanvas->backgroundA0);
    } else {
        // Free background parameters initialized from range endpoints (GASP standard)
        fullFunction->SetParameter(0, mainCanvas->backgroundA1);
        fullFunction->SetParameter(1, mainCanvas->backgroundA0);
    }

    // Initialize individual peak amplitudes, centroids, and widths
    for (std::size_t i = 0; i < nPeaks; ++i) {
        const Int_t peakChannel = static_cast<Int_t>(mainCanvas->gauss_markers[i] - 1);
        const Double_t rawCounts = activeHist->GetBinContent(peakChannel);
        const Double_t estBkg = mainCanvas->backgroundA1 * peakChannel + mainCanvas->backgroundA0;
        const Double_t initAmp = std::max(1.0, rawCounts - estBkg);
        const int ampIdx  = decoupleWidths ? (2 + 3 * static_cast<int>(i)) : (3 + 2 * static_cast<int>(i));
        const int meanIdx = decoupleWidths ? (3 + 3 * static_cast<int>(i)) : (4 + 2 * static_cast<int>(i));

        // Amplitude
        if (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixAmp) {
            fullFunction->FixParameter(ampIdx, mainCanvas->m_peakFixedStates[i].ampVal);
        } else {
            fullFunction->SetParameter(ampIdx, initAmp);
            fullFunction->SetParLimits(ampIdx, 0.0, maxValue * 1.5);
        }

        // Centroid
        if (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixCentroid) {
            double fixedCenterCh = mainCanvas->m_peakFixedStates[i].centroidVal;
            if (isCalib) {
                fixedCenterCh = trackHist->EnergyToChannel(fixedCenterCh);
            }
            fullFunction->FixParameter(meanIdx, fixedCenterCh);
        } else {
            fullFunction->SetParameter(meanIdx, peakChannel);
            fullFunction->SetParLimits(meanIdx, r0, r1);
        }

        // Decoupled width
        if (decoupleWidths) {
            const int sigIdx = 4 + 3 * static_cast<int>(i);
            if (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixWidth) {
                double fixedW = mainCanvas->m_peakFixedStates[i].widthVal;
                double fixedSigmaCh = 0.0;
                if (isCalib) {
                    double peakE = trackHist->ChannelToEnergy(peakChannel);
                    if (mainCanvas->m_peakFixedStates[i].fixCentroid) {
                        peakE = mainCanvas->m_peakFixedStates[i].centroidVal;
                    }
                    const Double_t ch1 = trackHist->EnergyToChannel(peakE - fixedW * 0.5);
                    const Double_t ch2 = trackHist->EnergyToChannel(peakE + fixedW * 0.5);
                    fixedSigmaCh = std::abs(ch2 - ch1) / 2.35482;
                } else {
                    fixedSigmaCh = fixedW / 2.35482;
                }
                fullFunction->FixParameter(sigIdx, fixedSigmaCh);
            } else {
                fullFunction->SetParameter(sigIdx, 3.0);
                fullFunction->SetParLimits(sigIdx, 0.4, std::abs(r1 - r0) * 2.0);
            }
        }
    }

    // Run Minuit fit
    TFitResultPtr fitResult = activeHist->Fit(fullFunction, "Q M R S", "same");

    // Adaptive retry logic if fit failed to converge (status code 4)
    if (static_cast<int>(fitResult) == 4) {
        ROOT::Math::MinimizerOptions::SetDefaultStrategy(2);
        ROOT::Math::MinimizerOptions::SetDefaultTolerance(0.1);
        ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(10000000);

        fitResult = activeHist->Fit(fullFunction, "Q M R S", "same");

        if (static_cast<int>(fitResult) == 4) {
            ROOT::Math::MinimizerOptions::SetDefaultTolerance(1.0);
            fitResult = activeHist->Fit(fullFunction, "Q M R S", "same");

            if (static_cast<int>(fitResult) == 4) {
                ROOT::Math::MinimizerOptions::SetDefaultTolerance(10.0);
                fitResult = activeHist->Fit(fullFunction, "Q M R S", "same");

                if (static_cast<int>(fitResult) == 4) {
                    const QString failMsg = "The fit has failed to converge despite adaptive retries. Some errors will not be calculated.\n";
                    std::cout << failMsg.toStdString();
                    CommandPrompt::getInstance()->appendPlainText(failMsg);
                }
            }
        }

        // Restore default minimizer settings
        ROOT::Math::MinimizerOptions::SetDefaultStrategy(1);
        ROOT::Math::MinimizerOptions::SetDefaultTolerance(0.01);
        ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(1630);
    }

    // Post-fit evaluation for free background (when no explicit 'B' markers were provided and not fixed)
    if (!hasExplicitBkg && !mainCanvas->m_bkgFixed) {
        mainCanvas->backgroundA1 = fullFunction->GetParameter(0);
        mainCanvas->backgroundA0 = fullFunction->GetParameter(1);
        mainCanvas->m_bkgSlopeVal = mainCanvas->backgroundA1;
        mainCanvas->m_bkgInterceptVal = mainCanvas->backgroundA0;
    }

    TF1 bkgTF1("bkgTF1", "[0]*x + [1]", r0, r1);
    bkgTF1.SetParameter(0, mainCanvas->backgroundA1);
    bkgTF1.SetParameter(1, mainCanvas->backgroundA0);
    mainCanvas->backgroundIntegral = bkgTF1.Integral(r0, r1);

    if (fitResult.Get() && fitResult->IsValid() && !hasExplicitBkg && !mainCanvas->m_bkgFixed) {
        TMatrixDSym bkgCov(2);
        bkgCov(0, 0) = fitResult->GetCovarianceMatrix()(0, 0);
        bkgCov(0, 1) = fitResult->GetCovarianceMatrix()(0, 1);
        bkgCov(1, 0) = fitResult->GetCovarianceMatrix()(1, 0);
        bkgCov(1, 1) = fitResult->GetCovarianceMatrix()(1, 1);
        mainCanvas->backgroundIntegralError = bkgTF1.IntegralError(
            r0, r1, bkgTF1.GetParameters(), bkgCov.GetMatrixArray());
    } else if (mainCanvas->m_bkgFixed) {
        mainCanvas->backgroundIntegralError = 0.0;
    }

    // Ensure mainCanvas->backgroundFunction is updated for zoom HUD
    if (!mainCanvas->backgroundFunction) {
        mainCanvas->backgroundFunction = new TF1("backgroundFunction", "[0]*x+[1]", 0, 10240);
    }
    mainCanvas->backgroundFunction->SetRange(r0, r1);
    mainCanvas->backgroundFunction->SetParameter(0, mainCanvas->backgroundA1);
    mainCanvas->backgroundFunction->SetParameter(1, mainCanvas->backgroundA0);

    // Draw fitted or fixed baseline line on canvas
    const Double_t xStart = r0 - 0.5;
    const Double_t xEnd   = r1 - 0.5;
    const Double_t yStart = mainCanvas->backgroundA1 * xStart + mainCanvas->backgroundA0;
    const Double_t yEnd   = mainCanvas->backgroundA1 * xEnd   + mainCanvas->backgroundA0;

    if (mainCanvas->multiPeakBkgLine) {
        if (mainCanvas->listOfObjectsDrawnOnScreen.FindObject(mainCanvas->multiPeakBkgLine)) {
            mainCanvas->listOfObjectsDrawnOnScreen.Remove(mainCanvas->multiPeakBkgLine);
            delete mainCanvas->multiPeakBkgLine;
        }
        mainCanvas->multiPeakBkgLine = nullptr;
    }

    mainCanvas->multiPeakBkgLine = new TLine(xStart, yStart, xEnd, yEnd);
    mainCanvas->multiPeakBkgLine->SetLineColor(kBlue);
    mainCanvas->multiPeakBkgLine->SetLineWidth(2);
    mainCanvas->multiPeakBkgLine->Draw("same");
    mainCanvas->listOfObjectsDrawnOnScreen.Add(mainCanvas->multiPeakBkgLine);

    // Calculate fit quality metrics: RMS residual and Mean Relative Residual (%) over [r0, r1]
    Double_t sumSqDiff = 0.0;
    Double_t sumRelDiff = 0.0;
    Int_t nBinsFit = 0;
    const Int_t bMin = std::max(1, static_cast<int>(std::round(r0)));
    const Int_t bMax = std::min(activeHist->GetNbinsX(), static_cast<int>(std::round(r1)));
    for (Int_t b = bMin; b <= bMax; ++b) {
        Double_t x = activeHist->GetBinCenter(b);
        Double_t y = activeHist->GetBinContent(b);
        Double_t fy = fullFunction->Eval(x);
        Double_t diff = y - fy;
        sumSqDiff += diff * diff;
        if (y > 0.0) {
            sumRelDiff += std::abs(diff) / y;
        }
        nBinsFit++;
    }
    Double_t rmsResidual = (nBinsFit > 0) ? std::sqrt(sumSqDiff / nBinsFit) : 0.0;
    Double_t meanRelResidualPct = (nBinsFit > 0) ? (sumRelDiff / nBinsFit * 100.0) : 0.0;

    // Print table header: if calibrated, show ONLY Energy; if uncalibrated, show ONLY Channels
    if (isCalib) {
        std::cout << std::left
                  << std::setw(10) << "Peak#"
                  << std::setw(18) << "Energy"
                  << std::setw(25) << "Area"
                  << std::setw(15) << "Width" << std::endl;

        QString headerRow = QString("%1%2%3%4")
            .arg("Peak#",  -10, QChar(' '))
            .arg("Energy", -18, QChar(' '))
            .arg("Area",   -25, QChar(' '))
            .arg("Width",  -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(headerRow);
    } else {
        std::cout << std::left
                  << std::setw(10) << "Peak#"
                  << std::setw(18) << "Channel"
                  << std::setw(25) << "Area"
                  << std::setw(15) << "Width" << std::endl;

        QString headerRow = QString("%1%2%3%4")
            .arg("Peak#",   -10, QChar(' '))
            .arg("Channel", -18, QChar(' '))
            .arg("Area",    -25, QChar(' '))
            .arg("Width",   -15, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(headerRow);
    }

    // Prepare HTML report for the unfocused parameters dialog
    Double_t totalChi2 = fitResult.Get() ? fitResult->Chi2() : fullFunction->GetChisquare();
    Int_t ndf = fitResult.Get() ? fitResult->Ndf() : fullFunction->GetNDF();
    Double_t redChi2 = (ndf > 0) ? (totalChi2 / ndf) : 0.0;

    QString redChi2Str = (redChi2 >= 1e4) ? QString::number(redChi2, 'g', 4) : QString::number(redChi2, 'f', 2);

    QString html = "<div style='color:#abb2bf; font-family: monospace; font-size:11px;'>";
    html += "<div style='margin-bottom:2px;'>";
    html += "<span style='color:#61afef; font-weight:bold;'>&#9654; Fit Quality</span><br>";
    html += QString("&nbsp;&bull; Reduced &chi;&sup2; (&chi;&sup2;/NDF): <b style='color:#98c379;'>%1</b><br>").arg(redChi2Str);
    html += QString("&nbsp;&bull; Mean Residual: <b style='color:#61afef;'>%1%</b> (RMS: %2 cts)<br>")
                .arg(meanRelResidualPct, 0, 'f', 1).arg(rmsResidual, 0, 'f', 0);
    html += "</div>";
    html += "</div>";

    // Clear previous peak centers for this histogram before recording new fit results
    mainCanvas->gaussCenters[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].clear();
    mainCanvas->gaussCentersHeight[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].clear();

    if (mainCanvas->m_isRefitting && mainCanvas->m_lastMultiPeakCount > 0 &&
        mainCanvas->puncte_calib2p.size() >= mainCanvas->m_lastMultiPeakCount) {
        mainCanvas->puncte_calib2p.erase(
            mainCanvas->puncte_calib2p.end() - mainCanvas->m_lastMultiPeakCount,
            mainCanvas->puncte_calib2p.end());
    }

    std::vector<FittedPeakData> peaks(nPeaks);
    std::vector<double> peakCenters(nPeaks);

    // Calculate individual peak integrals, uncertainties, and print data rows
    for (std::size_t i = 0; i < nPeaks; ++i) {
        const int ampIdx  = decoupleWidths ? (2 + 3 * static_cast<int>(i)) : (3 + 2 * static_cast<int>(i));
        const int meanIdx = decoupleWidths ? (3 + 3 * static_cast<int>(i)) : (4 + 2 * static_cast<int>(i));
        const int sigIdx  = decoupleWidths ? (4 + 3 * static_cast<int>(i)) : 2;

        const Double_t curSigma    = std::abs(fullFunction->GetParameter(sigIdx));
        const Double_t curSigmaErr = fullFunction->GetParError(sigIdx);
        const Double_t curFWHM     = curSigma * 2.35482;
        const Double_t curFwhmErr  = curSigmaErr * 2.35482;

        TF1 tempGaussFunction("tempGaussFunction", "gaus", r0, r1);
        tempGaussFunction.SetParameter(0, fullFunction->GetParameter(ampIdx));
        tempGaussFunction.SetParError(0, fullFunction->GetParError(ampIdx));
        tempGaussFunction.SetParameter(1, fullFunction->GetParameter(meanIdx));
        tempGaussFunction.SetParError(1, fullFunction->GetParError(meanIdx));
        tempGaussFunction.SetParameter(2, curSigma);
        tempGaussFunction.SetParError(2, curSigmaErr);

        Double_t fitIntegralError = 0.0;
        if (fitResult.Get() && fitResult->IsValid()) {
            TMatrixDSym peakCov(3);
            const int pIndices[3] = { ampIdx, meanIdx, sigIdx };
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    peakCov(r, c) = fitResult->GetCovarianceMatrix()(pIndices[r], pIndices[c]);
                }
            }
            fitIntegralError = tempGaussFunction.IntegralError(
                r0, r1, tempGaussFunction.GetParameters(), peakCov.GetMatrixArray());
        }

        // Combined uncertainty: Gaussian peak integral error + background uncertainty
        const Double_t combinedAreaError = std::sqrt(
            fitIntegralError * fitIntegralError +
            mainCanvas->backgroundIntegralError * mainCanvas->backgroundIntegralError);

        const Double_t peakCenter = fullFunction->GetParameter(meanIdx);
        const Double_t centerErr  = fullFunction->GetParError(meanIdx);
        const Double_t peakArea   = tempGaussFunction.Integral(r0, r1);
        peakCenters[i] = peakCenter;

        Double_t dispEnergy = peakCenter;
        Double_t dispEnergyErr = centerErr;
        Double_t dispFWHM = curFWHM;
        Double_t dispFwhmErr = curFwhmErr;

        if (isCalib) {
            dispEnergy = trackHist->ChannelToEnergy(peakCenter);
            dispEnergyErr = std::abs(trackHist->ChannelToEnergy(peakCenter + centerErr) - dispEnergy);

            // Energy calibrated FWHM matching XTrackN standard:
            // |E(center + FWHM/2) - E(center - FWHM/2)|
            dispFWHM = std::abs(trackHist->ChannelToEnergy(peakCenter + curFWHM * 0.5) -
                                trackHist->ChannelToEnergy(peakCenter - curFWHM * 0.5));
            dispFwhmErr = (curFWHM > 0.0) ? (curFwhmErr * dispFWHM / curFWHM) : 0.0;
        }

        QString numberStr = QString::number(i + 1).leftJustified(10, ' ');

        QString areaStr;
        if (std::isnan(peakArea) || std::isinf(peakArea)) {
            areaStr = "0(0)";
        } else {
            long long roundedErr = (std::isnan(combinedAreaError) || std::isinf(combinedAreaError)) ? 0 : qRound(combinedAreaError);
            areaStr = QString("%1(%2)").arg(peakArea, 0, 'f', 0).arg(roundedErr);
        }

        long long roundedFwhmErr = (std::isnan(dispFwhmErr) || std::isinf(dispFwhmErr)) ? 0 : qCeil(dispFwhmErr * 100);
        QString fwhmStr = QString("%1(%2)").arg(dispFWHM, 0, 'f', 2).arg(roundedFwhmErr);

        Double_t peakHeight = fullFunction->Eval(peakCenter);
        mainCanvas->gaussCenters[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(peakCenter);
        mainCanvas->gaussCentersHeight[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j].push_back(peakHeight);
        mainCanvas->puncte_calib2p.push_back(peakCenter);

        if (isCalib) {
            long long roundedEnergyErr = (std::isnan(dispEnergyErr) || std::isinf(dispEnergyErr)) ? 0 : qCeil(dispEnergyErr * 100);
            QString energyStr = QString("%1(%2)").arg(dispEnergy, 0, 'f', 2).arg(roundedEnergyErr);

            QString dataRow = QString("%1%2%3%4")
                .arg(numberStr)
                .arg(energyStr, -18, QChar(' '))
                .arg(areaStr,   -25, QChar(' '))
                .arg(fwhmStr,   -15, QChar(' '));
            CommandPrompt::getInstance()->appendPlainText(dataRow);

            std::cout << std::left
                      << std::setw(10) << (i + 1)
                      << std::setw(18) << energyStr.toStdString()
                      << std::setw(25) << areaStr.toStdString()
                      << std::setw(15) << fwhmStr.toStdString() << std::endl;
        } else {
            long long roundedCenterErr = (std::isnan(centerErr) || std::isinf(centerErr)) ? 0 : qCeil(centerErr * 100);
            QString centerWithErrStr = QString("%1(%2)").arg(peakCenter, 0, 'f', 2).arg(roundedCenterErr);

            QString dataRow = QString("%1%2%3%4")
                .arg(numberStr)
                .arg(centerWithErrStr, -18, QChar(' '))
                .arg(areaStr,          -25, QChar(' '))
                .arg(fwhmStr,          -15, QChar(' '));
            CommandPrompt::getInstance()->appendPlainText(dataRow);

            std::cout << std::left
                      << std::setw(10) << (i + 1)
                      << std::setw(18) << centerWithErrStr.toStdString()
                      << std::setw(25) << areaStr.toStdString()
                      << std::setw(15) << fwhmStr.toStdString() << std::endl;
        }

        // Record fitted peak data for interactive peak dialog cards
        peaks[i].peakIndex = static_cast<int>(i);
        peaks[i].centroid = dispEnergy;
        peaks[i].centroidErr = dispEnergyErr;
        peaks[i].amplitude = fullFunction->GetParameter(ampIdx);
        peaks[i].amplitudeErr = fullFunction->GetParError(ampIdx);
        peaks[i].width = dispFWHM;
        peaks[i].widthErr = dispFwhmErr;
        peaks[i].netArea = peakArea;
        peaks[i].netAreaErr = combinedAreaError;
        peaks[i].isCalibrated = isCalib;
    }
    mainCanvas->m_lastMultiPeakCount = nPeaks;

    if (mainCanvas->m_isAreaLoggingEnabled && nPeaks > 0) {
        mainCanvas->writeAreaLogHeader(true);
        TF1 bkgTF1_log("bkgTF1_log", "[0]*x + [1]", r0, r1);
        bkgTF1_log.SetParameter(0, mainCanvas->backgroundA1);
        bkgTF1_log.SetParameter(1, mainCanvas->backgroundA0);

        for (std::size_t i = 0; i < nPeaks; ++i) {
            double bkgForPeak = 0.0;
            if (nPeaks == 1) {
                bkgForPeak = mainCanvas->backgroundIntegral;
            } else {
                double pMin = (i == 0) ? r0 : 0.5 * (peakCenters[i - 1] + peakCenters[i]);
                double pMax = (i + 1 == nPeaks) ? r1 : 0.5 * (peakCenters[i] + peakCenters[i + 1]);
                if (pMax > pMin) {
                    bkgForPeak = bkgTF1_log.Integral(pMin, pMax);
                }
            }
            double grossArea = peaks[i].netArea + bkgForPeak;
            mainCanvas->writeAreaLogData(peaks[i].centroid, peaks[i].width, grossArea, peaks[i].netArea, bkgForPeak, peaks[i].netAreaErr);
        }
    }

    CommandPrompt::getInstance()->appendPlainText("");
    mainCanvas->renderPeakLabels(mainCanvas->SelectedElement_i, mainCanvas->SelectedElement_j);

    mainCanvas->canvas->getCanvas()->Modified();
    mainCanvas->canvas->getCanvas()->Update();

    // Display unfocused floating dialog with all parameters docked in top right corner
    showFitParametersDialog(mainCanvas, "Multi-Peak Fit Parameters", html, peaks);
}

// Helper to ensure crisp spinbox arrow icons are available on disk
static void ensureSpinArrowIcons(QString &upPath, QString &downPath) {
    QString dir = QDir::tempPath();
    upPath = dir + "/nutrackn_spin_up.png";
    downPath = dir + "/nutrackn_spin_down.png";
    if (!QFile::exists(upPath)) {
        QPixmap upPix(9, 6);
        upPix.fill(Qt::transparent);
        QPainter p(&upPix);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#dcdfe4"));
        QPolygon poly;
        poly << QPoint(4, 0) << QPoint(8, 5) << QPoint(0, 5);
        p.drawPolygon(poly);
        upPix.save(upPath);
    }
    if (!QFile::exists(downPath)) {
        QPixmap downPix(9, 6);
        downPix.fill(Qt::transparent);
        QPainter p(&downPix);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#dcdfe4"));
        QPolygon poly;
        poly << QPoint(0, 0) << QPoint(8, 0) << QPoint(4, 5);
        p.drawPolygon(poly);
        downPix.save(downPath);
    }
}

//==============================================================================
// showFitParametersDialog
//==============================================================================
// Displays an unfocused floating parameters dialog docked in the top-right corner
// of the spectrum canvas, presenting Chi^2, background parameters, centroids,
// amplitudes, areas, and calibrated widths.
//==============================================================================
void showFitParametersDialog(QMainCanvas *mainCanvas, const QString &title, const QString &htmlContent, const std::vector<FittedPeakData> &peaks)
{
    if (!mainCanvas) return;

    QString upArrowPath, downArrowPath;
    ensureSpinArrowIcons(upArrowPath, downArrowPath);

    if (!mainCanvas->fitParamsDialog) {
        mainCanvas->fitParamsDialog = new QDialog(mainCanvas, Qt::Tool | Qt::WindowStaysOnTopHint);
        mainCanvas->fitParamsDialog->setAttribute(Qt::WA_ShowWithoutActivating, true);
        mainCanvas->fitParamsDialog->setFocusPolicy(Qt::NoFocus);
        mainCanvas->fitParamsDialog->setStyleSheet(QString(
            "QDialog, QWidget {"
            "    background-color: #1e1e24;"
            "    color: #e0e0e0;"
            "}"
            "QDialog {"
            "    border: 1px solid #4f5b66;"
            "    border-radius: 6px;"
            "}"
            "QTextBrowser {"
            "    background-color: #181a1f;"
            "    color: #dcdfe4;"
            "    border: 1px solid #2c313a;"
            "    border-radius: 4px;"
            "    font-family: 'Consolas', 'DejaVu Sans Mono', 'Monaco', monospace;"
            "    font-size: 11px;"
            "    padding: 6px;"
            "}"
            "QGroupBox {"
            "    background-color: transparent;"
            "    border: 1px solid #3e4451;"
            "    border-radius: 5px;"
            "    margin-top: 8px;"
            "    padding: 8px 6px 6px 6px;"
            "    font-size: 11px;"
            "    font-weight: bold;"
            "    color: #61afef;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    subcontrol-position: top left;"
            "    left: 8px;"
            "    padding: 0 4px;"
            "}"
            "QDoubleSpinBox {"
            "    background-color: #181a1f;"
            "    color: #e5c07b;"
            "    border: 1px solid #3e4451;"
            "    border-radius: 3px;"
            "    padding: 2px 22px 2px 4px;"
            "    font-family: 'Consolas', 'DejaVu Sans Mono', 'Monaco', monospace;"
            "    font-size: 11px;"
            "}"
            "QDoubleSpinBox:focus {"
            "    border: 1px solid #61afef;"
            "}"
            "QDoubleSpinBox:disabled {"
            "    background-color: #21252b;"
            "    color: #5c6370;"
            "    border: 1px solid #2c313a;"
            "}"
            "QDoubleSpinBox::up-button {"
            "    subcontrol-origin: border;"
            "    subcontrol-position: top right;"
            "    width: 18px;"
            "    height: 11px;"
            "    background-color: #2c313a;"
            "    border-left: 1px solid #3e4451;"
            "    border-bottom: 1px solid #3e4451;"
            "}"
            "QDoubleSpinBox::up-button:hover {"
            "    background-color: #3e4451;"
            "}"
            "QDoubleSpinBox::up-arrow {"
            "    image: url(%1);"
            "    width: 8px;"
            "    height: 5px;"
            "}"
            "QDoubleSpinBox::down-button {"
            "    subcontrol-origin: border;"
            "    subcontrol-position: bottom right;"
            "    width: 18px;"
            "    height: 11px;"
            "    background-color: #2c313a;"
            "    border-left: 1px solid #3e4451;"
            "}"
            "QDoubleSpinBox::down-button:hover {"
            "    background-color: #3e4451;"
            "}"
            "QDoubleSpinBox::down-arrow {"
            "    image: url(%2);"
            "    width: 8px;"
            "    height: 5px;"
            "}"
            "QLabel {"
            "    background-color: transparent;"
            "    color: #abb2bf;"
            "    font-size: 11px;"
            "}"
            "QCheckBox {"
            "    background-color: transparent;"
            "    color: #abb2bf;"
            "    font-size: 11px;"
            "}"
            "QCheckBox::indicator {"
            "    width: 13px;"
            "    height: 13px;"
            "}"
            "QPushButton {"
            "    background-color: #2c313a;"
            "    color: #abb2bf;"
            "    border: 1px solid #3e4451;"
            "    border-radius: 4px;"
            "    padding: 4px 14px;"
            "    font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #3e4451;"
            "    color: #ffffff;"
            "}"
        ).arg(upArrowPath, downArrowPath));

        QVBoxLayout *layout = new QVBoxLayout(mainCanvas->fitParamsDialog);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        mainCanvas->fitParamsBrowser = new QTextBrowser(mainCanvas->fitParamsDialog);
        mainCanvas->fitParamsBrowser->setFocusPolicy(Qt::NoFocus);
        mainCanvas->fitParamsBrowser->setOpenExternalLinks(false);
        layout->addWidget(mainCanvas->fitParamsBrowser);

        // Background parameter controls group
        QGroupBox *bkgGroup = new QGroupBox("Background Parameters", mainCanvas->fitParamsDialog);
        QVBoxLayout *bkgLayout = new QVBoxLayout(bkgGroup);
        bkgLayout->setContentsMargins(8, 6, 8, 6);
        bkgLayout->setSpacing(5);

        mainCanvas->chkFixBackground = new QCheckBox("Fix background parameters", bkgGroup);
        mainCanvas->chkFixBackground->setFocusPolicy(Qt::NoFocus);
        mainCanvas->chkFixBackground->setToolTip("Lock background slope and intercept during fit");
        bkgLayout->addWidget(mainCanvas->chkFixBackground);

        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 2, 0, 0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);

        QLabel *lblSlope = new QLabel("Slope (A1):", bkgGroup);
        mainCanvas->spinBkgSlope = new QDoubleSpinBox(bkgGroup);
        mainCanvas->spinBkgSlope->setRange(-1e9, 1e9);
        mainCanvas->spinBkgSlope->setDecimals(6);
        mainCanvas->spinBkgSlope->setSingleStep(0.001);
        mainCanvas->spinBkgSlope->setEnabled(false);

        QLabel *lblInt = new QLabel("Intercept (A0):", bkgGroup);
        mainCanvas->spinBkgIntercept = new QDoubleSpinBox(bkgGroup);
        mainCanvas->spinBkgIntercept->setRange(-1e9, 1e9);
        mainCanvas->spinBkgIntercept->setDecimals(2);
        mainCanvas->spinBkgIntercept->setSingleStep(1.0);
        mainCanvas->spinBkgIntercept->setEnabled(false);

        grid->addWidget(lblSlope, 0, 0);
        grid->addWidget(mainCanvas->spinBkgSlope, 0, 1);
        grid->addWidget(lblInt, 1, 0);
        grid->addWidget(mainCanvas->spinBkgIntercept, 1, 1);
        bkgLayout->addLayout(grid);

        layout->addWidget(bkgGroup);

        // Fitted peaks container directly in dialog layout without scroll bar
        mainCanvas->peaksContainer = new QWidget(mainCanvas->fitParamsDialog);
        mainCanvas->peaksLayout = new QVBoxLayout(mainCanvas->peaksContainer);
        mainCanvas->peaksLayout->setContentsMargins(0, 0, 0, 0);
        mainCanvas->peaksLayout->setSpacing(6);

        layout->addWidget(mainCanvas->peaksContainer);

        // Setup 1-second debounce timer
        mainCanvas->bkgDebounceTimer = new QTimer(mainCanvas);
        mainCanvas->bkgDebounceTimer->setSingleShot(true);
        QObject::connect(mainCanvas->bkgDebounceTimer, &QTimer::timeout, mainCanvas, [mainCanvas]() {
            if (!mainCanvas->fitParamsDialog || !mainCanvas->fitParamsDialog->isVisible()) return;

            bool fixBkg = mainCanvas->chkFixBackground && mainCanvas->chkFixBackground->isChecked();
            mainCanvas->m_bkgFixed = fixBkg;
            if (mainCanvas->spinBkgSlope) {
                mainCanvas->m_bkgSlopeVal = mainCanvas->spinBkgSlope->value();
            }
            if (mainCanvas->spinBkgIntercept) {
                mainCanvas->m_bkgInterceptVal = mainCanvas->spinBkgIntercept->value();
            }

            mainCanvas->m_isRefitting = true;
            if (mainCanvas->m_lastFitType == 1) {
                runAutoFit(mainCanvas, mainCanvas->m_lastAutoFitX, mainCanvas->m_lastAutoFitY);
            } else {
                runMultiPeakFit(mainCanvas);
            }
            mainCanvas->m_isRefitting = false;
        });

        // Trigger debounce on background spinbox or checkbox change
        auto restartDebounceTimer = [mainCanvas]() {
            if (mainCanvas->spinBkgSlope) {
                mainCanvas->m_bkgSlopeVal = mainCanvas->spinBkgSlope->value();
            }
            if (mainCanvas->spinBkgIntercept) {
                mainCanvas->m_bkgInterceptVal = mainCanvas->spinBkgIntercept->value();
            }
            mainCanvas->bkgDebounceTimer->start(1000);
        };

        QObject::connect(mainCanvas->chkFixBackground, &QCheckBox::toggled, mainCanvas, [mainCanvas, restartDebounceTimer](bool checked) {
            mainCanvas->m_bkgFixed = checked;
            if (mainCanvas->spinBkgSlope) {
                mainCanvas->spinBkgSlope->setEnabled(checked);
            }
            if (mainCanvas->spinBkgIntercept) {
                mainCanvas->spinBkgIntercept->setEnabled(checked);
            }
            restartDebounceTimer();
        });

        QObject::connect(mainCanvas->spinBkgSlope, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [mainCanvas, restartDebounceTimer](double) {
            if (mainCanvas->m_bkgFixed) {
                restartDebounceTimer();
            }
        });

        QObject::connect(mainCanvas->spinBkgIntercept, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [mainCanvas, restartDebounceTimer](double) {
            if (mainCanvas->m_bkgFixed) {
                restartDebounceTimer();
            }
        });

        // Bottom row with uncouple width and close button
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(8);

        mainCanvas->chkUncoupleWidths = new QCheckBox("Uncouple width", mainCanvas->fitParamsDialog);
        mainCanvas->chkUncoupleWidths->setFocusPolicy(Qt::NoFocus);
        mainCanvas->chkUncoupleWidths->setToolTip("Uncouple peak widths to fit independent widths per peak");
        mainCanvas->chkUncoupleWidths->setChecked(mainCanvas->m_uncoupleWidths);

        QObject::connect(mainCanvas->chkUncoupleWidths, &QCheckBox::toggled, mainCanvas, [mainCanvas](bool checked) {
            mainCanvas->m_isRefitting = true;
            mainCanvas->m_uncoupleWidths = checked;
            runMultiPeakFit(mainCanvas);
            mainCanvas->m_isRefitting = false;
        });

        btnLayout->addWidget(mainCanvas->chkUncoupleWidths);
        btnLayout->addStretch();
        QPushButton *btnClose = new QPushButton("Close", mainCanvas->fitParamsDialog);
        btnClose->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnClose, &QPushButton::clicked, mainCanvas->fitParamsDialog, &QDialog::close);
        btnLayout->addWidget(btnClose);
        layout->addLayout(btnLayout);
    }

    const bool isMultiPeak = title.contains("Multi-Peak", Qt::CaseInsensitive);
    if (mainCanvas->chkUncoupleWidths) {
        mainCanvas->chkUncoupleWidths->setVisible(isMultiPeak);
        mainCanvas->chkUncoupleWidths->blockSignals(true);
        mainCanvas->chkUncoupleWidths->setChecked(mainCanvas->m_uncoupleWidths);
        mainCanvas->chkUncoupleWidths->blockSignals(false);
    }

    // Update background controls with 1% step precision of the fitted values
    const double slopeVal = mainCanvas->backgroundA1;
    const double interceptVal = mainCanvas->backgroundA0;

    double slopeStep = std::abs(slopeVal) * 0.01;
    if (slopeStep < 1e-6) slopeStep = 0.001;

    int slopeDecimals = 4;
    if (slopeStep > 0.0) {
        int dec = static_cast<int>(std::ceil(-std::log10(slopeStep))) + 1;
        slopeDecimals = std::max(4, std::min(8, dec));
    }

    double interceptStep = std::abs(interceptVal) * 0.01;
    if (interceptStep < 0.01) interceptStep = 1.0;

    const bool isFixed = mainCanvas->m_bkgFixed;

    if (mainCanvas->spinBkgSlope) {
        mainCanvas->spinBkgSlope->blockSignals(true);
        mainCanvas->spinBkgSlope->setDecimals(slopeDecimals);
        mainCanvas->spinBkgSlope->setSingleStep(slopeStep);
        mainCanvas->spinBkgSlope->setValue(slopeVal);
        mainCanvas->spinBkgSlope->setEnabled(isFixed);
        mainCanvas->spinBkgSlope->setToolTip(QString("Background slope (A1). Step (1%): %1").arg(slopeStep, 0, 'g', 3));
        mainCanvas->spinBkgSlope->blockSignals(false);
    }

    if (mainCanvas->spinBkgIntercept) {
        mainCanvas->spinBkgIntercept->blockSignals(true);
        mainCanvas->spinBkgIntercept->setDecimals(2);
        mainCanvas->spinBkgIntercept->setSingleStep(interceptStep);
        mainCanvas->spinBkgIntercept->setValue(interceptVal);
        mainCanvas->spinBkgIntercept->setEnabled(isFixed);
        mainCanvas->spinBkgIntercept->setToolTip(QString("Background intercept (A0). Step (1%): %1").arg(interceptStep, 0, 'f', 2));
        mainCanvas->spinBkgIntercept->blockSignals(false);
    }

    if (mainCanvas->chkFixBackground) {
        mainCanvas->chkFixBackground->blockSignals(true);
        mainCanvas->chkFixBackground->setChecked(isFixed);
        mainCanvas->chkFixBackground->blockSignals(false);
    }

    auto restartDebounceTimer = [mainCanvas]() {
        if (mainCanvas->bkgDebounceTimer) {
            mainCanvas->bkgDebounceTimer->start(1000);
        }
    };

    const std::size_t nPeaks = peaks.size();
    if (mainCanvas->m_peakFixedStates.size() != nPeaks) {
        mainCanvas->m_peakFixedStates.resize(nPeaks);
    }

    // Dynamically expand peak UI control cards as needed
    while (mainCanvas->m_peakUIControls.size() < nPeaks) {
        const int idx = static_cast<int>(mainCanvas->m_peakUIControls.size());
        PeakUIControls ctrl;

        ctrl.groupBox = new QGroupBox(QString("Peak #%1").arg(idx + 1), mainCanvas->peaksContainer);
        QVBoxLayout *grpLayout = new QVBoxLayout(ctrl.groupBox);
        grpLayout->setContentsMargins(8, 6, 8, 6);
        grpLayout->setSpacing(4);

        ctrl.lblNetArea = new QLabel(ctrl.groupBox);
        ctrl.lblNetArea->setStyleSheet("color: #abb2bf; font-size: 11px; font-weight: normal;");
        grpLayout->addWidget(ctrl.lblNetArea);

        QGridLayout *pGrid = new QGridLayout();
        pGrid->setContentsMargins(0, 2, 0, 0);
        pGrid->setHorizontalSpacing(8);
        pGrid->setVerticalSpacing(4);

        // Centroid row
        QLabel *lblCent = new QLabel("Centroid:", ctrl.groupBox);
        ctrl.chkFixCentroid = new QCheckBox("Fix", ctrl.groupBox);
        ctrl.chkFixCentroid->setFocusPolicy(Qt::NoFocus);
        ctrl.chkFixCentroid->setToolTip(QString("Lock peak #%1 centroid during fit").arg(idx + 1));
        ctrl.spinCentroid = new QDoubleSpinBox(ctrl.groupBox);
        ctrl.spinCentroid->setRange(-1e7, 1e7);
        ctrl.spinCentroid->setEnabled(false);

        pGrid->addWidget(lblCent, 0, 0);
        pGrid->addWidget(ctrl.chkFixCentroid, 0, 1);
        pGrid->addWidget(ctrl.spinCentroid, 0, 2);

        // Amplitude row
        QLabel *lblAmp = new QLabel("Amplitude:", ctrl.groupBox);
        ctrl.chkFixAmp = new QCheckBox("Fix", ctrl.groupBox);
        ctrl.chkFixAmp->setFocusPolicy(Qt::NoFocus);
        ctrl.chkFixAmp->setToolTip(QString("Lock peak #%1 amplitude during fit").arg(idx + 1));
        ctrl.spinAmp = new QDoubleSpinBox(ctrl.groupBox);
        ctrl.spinAmp->setRange(0.0, 1e9);
        ctrl.spinAmp->setEnabled(false);

        pGrid->addWidget(lblAmp, 1, 0);
        pGrid->addWidget(ctrl.chkFixAmp, 1, 1);
        pGrid->addWidget(ctrl.spinAmp, 1, 2);

        // Width row
        QLabel *lblW = new QLabel("Width:", ctrl.groupBox);
        ctrl.chkFixWidth = new QCheckBox("Fix", ctrl.groupBox);
        ctrl.chkFixWidth->setFocusPolicy(Qt::NoFocus);
        ctrl.chkFixWidth->setToolTip(QString("Lock peak #%1 width during fit").arg(idx + 1));
        ctrl.spinWidth = new QDoubleSpinBox(ctrl.groupBox);
        ctrl.spinWidth->setRange(0.01, 1e6);
        ctrl.spinWidth->setEnabled(false);

        pGrid->addWidget(lblW, 2, 0);
        pGrid->addWidget(ctrl.chkFixWidth, 2, 1);
        pGrid->addWidget(ctrl.spinWidth, 2, 2);

        pGrid->setColumnStretch(0, 0);
        pGrid->setColumnStretch(1, 0);
        pGrid->setColumnStretch(2, 1);

        grpLayout->addLayout(pGrid);
        mainCanvas->peaksLayout->addWidget(ctrl.groupBox);

        // Connect Centroid
        QObject::connect(ctrl.chkFixCentroid, &QCheckBox::toggled, mainCanvas, [mainCanvas, idx, restartDebounceTimer](bool checked) {
            if (idx < static_cast<int>(mainCanvas->m_peakFixedStates.size())) {
                mainCanvas->m_peakFixedStates[idx].fixCentroid = checked;
                if (idx < static_cast<int>(mainCanvas->m_peakUIControls.size()) && mainCanvas->m_peakUIControls[idx].spinCentroid) {
                    mainCanvas->m_peakFixedStates[idx].centroidVal = mainCanvas->m_peakUIControls[idx].spinCentroid->value();
                    mainCanvas->m_peakUIControls[idx].spinCentroid->setEnabled(checked);
                }
            }
            restartDebounceTimer();
        });

        QObject::connect(ctrl.spinCentroid, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [mainCanvas, idx, restartDebounceTimer](double val) {
            if (idx < static_cast<int>(mainCanvas->m_peakFixedStates.size()) && mainCanvas->m_peakFixedStates[idx].fixCentroid) {
                mainCanvas->m_peakFixedStates[idx].centroidVal = val;
                restartDebounceTimer();
            }
        });

        // Connect Amplitude
        QObject::connect(ctrl.chkFixAmp, &QCheckBox::toggled, mainCanvas, [mainCanvas, idx, restartDebounceTimer](bool checked) {
            if (idx < static_cast<int>(mainCanvas->m_peakFixedStates.size())) {
                mainCanvas->m_peakFixedStates[idx].fixAmp = checked;
                if (idx < static_cast<int>(mainCanvas->m_peakUIControls.size()) && mainCanvas->m_peakUIControls[idx].spinAmp) {
                    mainCanvas->m_peakFixedStates[idx].ampVal = mainCanvas->m_peakUIControls[idx].spinAmp->value();
                    mainCanvas->m_peakUIControls[idx].spinAmp->setEnabled(checked);
                }
            }
            restartDebounceTimer();
        });

        QObject::connect(ctrl.spinAmp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [mainCanvas, idx, restartDebounceTimer](double val) {
            if (idx < static_cast<int>(mainCanvas->m_peakFixedStates.size()) && mainCanvas->m_peakFixedStates[idx].fixAmp) {
                mainCanvas->m_peakFixedStates[idx].ampVal = val;
                restartDebounceTimer();
            }
        });

        // Connect Width with Coupled vs Uncoupled logic
        QObject::connect(ctrl.chkFixWidth, &QCheckBox::toggled, mainCanvas, [mainCanvas, idx, restartDebounceTimer](bool checked) {
            if (!mainCanvas->m_uncoupleWidths) {
                // Width is COUPLED: changing fixed one changes ALL others
                double curW = 0.0;
                if (idx < static_cast<int>(mainCanvas->m_peakUIControls.size()) && mainCanvas->m_peakUIControls[idx].spinWidth) {
                    curW = mainCanvas->m_peakUIControls[idx].spinWidth->value();
                }
                for (std::size_t k = 0; k < mainCanvas->m_peakFixedStates.size(); ++k) {
                    mainCanvas->m_peakFixedStates[k].fixWidth = checked;
                    mainCanvas->m_peakFixedStates[k].widthVal = curW;
                }
                for (std::size_t k = 0; k < mainCanvas->m_peakUIControls.size(); ++k) {
                    auto &c = mainCanvas->m_peakUIControls[k];
                    if (c.chkFixWidth) {
                        c.chkFixWidth->blockSignals(true);
                        c.chkFixWidth->setChecked(checked);
                        c.chkFixWidth->blockSignals(false);
                    }
                    if (c.spinWidth) {
                        c.spinWidth->setEnabled(checked);
                        c.spinWidth->blockSignals(true);
                        c.spinWidth->setValue(curW);
                        c.spinWidth->blockSignals(false);
                    }
                }
            } else {
                // Width is UNCOUPLED: only changes it for that peak
                if (idx < static_cast<int>(mainCanvas->m_peakFixedStates.size())) {
                    mainCanvas->m_peakFixedStates[idx].fixWidth = checked;
                    if (idx < static_cast<int>(mainCanvas->m_peakUIControls.size()) && mainCanvas->m_peakUIControls[idx].spinWidth) {
                        mainCanvas->m_peakFixedStates[idx].widthVal = mainCanvas->m_peakUIControls[idx].spinWidth->value();
                        mainCanvas->m_peakUIControls[idx].spinWidth->setEnabled(checked);
                    }
                }
            }
            restartDebounceTimer();
        });

        QObject::connect(ctrl.spinWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [mainCanvas, idx, restartDebounceTimer](double val) {
            if (idx >= static_cast<int>(mainCanvas->m_peakFixedStates.size()) || !mainCanvas->m_peakFixedStates[idx].fixWidth) {
                return;
            }
            if (!mainCanvas->m_uncoupleWidths) {
                // Width is COUPLED: changing fixed one changes ALL others
                for (std::size_t k = 0; k < mainCanvas->m_peakFixedStates.size(); ++k) {
                    mainCanvas->m_peakFixedStates[k].widthVal = val;
                }
                for (std::size_t k = 0; k < mainCanvas->m_peakUIControls.size(); ++k) {
                    if (k != static_cast<std::size_t>(idx) && mainCanvas->m_peakUIControls[k].spinWidth) {
                        mainCanvas->m_peakUIControls[k].spinWidth->blockSignals(true);
                        mainCanvas->m_peakUIControls[k].spinWidth->setValue(val);
                        mainCanvas->m_peakUIControls[k].spinWidth->blockSignals(false);
                    }
                }
            } else {
                // Width is UNCOUPLED: only changes it for that peak
                mainCanvas->m_peakFixedStates[idx].widthVal = val;
            }
            restartDebounceTimer();
        });

        mainCanvas->m_peakUIControls.push_back(ctrl);
    }

    // Populate or hide peak controls
    for (std::size_t i = 0; i < mainCanvas->m_peakUIControls.size(); ++i) {
        if (i < nPeaks) {
            mainCanvas->m_peakUIControls[i].groupBox->show();
            const FittedPeakData &pd = peaks[i];
            PeakUIControls &ctrl = mainCanvas->m_peakUIControls[i];

            QString areaStr;
            if (std::isnan(pd.netArea) || std::isinf(pd.netArea)) {
                areaStr = "Net Area: 0 ± 0";
            } else {
                long long roundedErr = (std::isnan(pd.netAreaErr) || std::isinf(pd.netAreaErr)) ? 0 : qRound(pd.netAreaErr);
                areaStr = QString("Net Area: %1 ± %2").arg(pd.netArea, 0, 'f', 0).arg(roundedErr);
            }
            ctrl.lblNetArea->setText(areaStr);

            const QString unitSuffix = pd.isCalibrated ? " keV" : " ch";
            ctrl.spinCentroid->setSuffix(unitSuffix);
            ctrl.spinAmp->setSuffix(" cts");
            ctrl.spinWidth->setSuffix(unitSuffix);

            double centStep = std::max(0.01, std::abs(pd.centroid) * 0.01);
            ctrl.spinCentroid->setDecimals(2);
            ctrl.spinCentroid->setSingleStep(centStep);
            ctrl.spinCentroid->setToolTip(QString("Centroid%1. Step (1%): %2").arg(unitSuffix).arg(centStep, 0, 'g', 3));

            double ampStep = std::max(1.0, std::abs(pd.amplitude) * 0.01);
            ctrl.spinAmp->setDecimals(1);
            ctrl.spinAmp->setSingleStep(ampStep);
            ctrl.spinAmp->setToolTip(QString("Amplitude (cts). Step (1%): %1").arg(ampStep, 0, 'f', 1));

            double widthStep = std::max(0.01, std::abs(pd.width) * 0.01);
            int widthDecimals = (widthStep < 0.1) ? 3 : 2;
            ctrl.spinWidth->setDecimals(widthDecimals);
            ctrl.spinWidth->setSingleStep(widthStep);
            ctrl.spinWidth->setToolTip(QString("Width (FWHM)%1. Step (1%): %2").arg(unitSuffix).arg(widthStep, 0, 'g', 3));

            bool fixCent = (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixCentroid);
            bool fixAmp  = (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixAmp);
            bool fixW    = (i < mainCanvas->m_peakFixedStates.size() && mainCanvas->m_peakFixedStates[i].fixWidth);

            ctrl.chkFixCentroid->blockSignals(true);
            ctrl.chkFixCentroid->setChecked(fixCent);
            ctrl.chkFixCentroid->blockSignals(false);
            ctrl.spinCentroid->setEnabled(fixCent);

            ctrl.chkFixAmp->blockSignals(true);
            ctrl.chkFixAmp->setChecked(fixAmp);
            ctrl.chkFixAmp->blockSignals(false);
            ctrl.spinAmp->setEnabled(fixAmp);

            ctrl.chkFixWidth->blockSignals(true);
            ctrl.chkFixWidth->setChecked(fixW);
            ctrl.chkFixWidth->blockSignals(false);
            ctrl.spinWidth->setEnabled(fixW);

            ctrl.spinCentroid->blockSignals(true);
            ctrl.spinCentroid->setValue(fixCent ? mainCanvas->m_peakFixedStates[i].centroidVal : pd.centroid);
            if (!fixCent && i < mainCanvas->m_peakFixedStates.size()) {
                mainCanvas->m_peakFixedStates[i].centroidVal = pd.centroid;
            }
            ctrl.spinCentroid->blockSignals(false);

            ctrl.spinAmp->blockSignals(true);
            ctrl.spinAmp->setValue(fixAmp ? mainCanvas->m_peakFixedStates[i].ampVal : pd.amplitude);
            if (!fixAmp && i < mainCanvas->m_peakFixedStates.size()) {
                mainCanvas->m_peakFixedStates[i].ampVal = pd.amplitude;
            }
            ctrl.spinAmp->blockSignals(false);

            ctrl.spinWidth->blockSignals(true);
            ctrl.spinWidth->setValue(fixW ? mainCanvas->m_peakFixedStates[i].widthVal : pd.width);
            if (!fixW && i < mainCanvas->m_peakFixedStates.size()) {
                mainCanvas->m_peakFixedStates[i].widthVal = pd.width;
            }
            ctrl.spinWidth->blockSignals(false);
        } else {
            mainCanvas->m_peakUIControls[i].groupBox->hide();
        }
    }

    mainCanvas->fitParamsDialog->setWindowTitle(title);

    // Auto-resize the dialog to content
    if (mainCanvas->fitParamsBrowser) {
        mainCanvas->fitParamsBrowser->setHtml(htmlContent);

        const int dialogWidth = 380;
        mainCanvas->fitParamsDialog->setFixedWidth(dialogWidth);

        // Compute document height to hug content without extra whitespace
        mainCanvas->fitParamsBrowser->document()->setTextWidth(dialogWidth - 32);
        mainCanvas->fitParamsBrowser->document()->adjustSize();
        int docHeight = std::ceil(mainCanvas->fitParamsBrowser->document()->size().height());
        mainCanvas->fitParamsBrowser->setFixedHeight(docHeight + 10);
        mainCanvas->fitParamsBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        mainCanvas->fitParamsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // Fit all data without any scroll bar
        mainCanvas->fitParamsDialog->resize(dialogWidth, 0);
        mainCanvas->fitParamsDialog->adjustSize();
    }

    // Always dock in the top-right corner of the canvas
    if (mainCanvas->canvas) {
        QPoint canvasTopRight = mainCanvas->canvas->mapToGlobal(QPoint(mainCanvas->canvas->width(), 0));
        int posX = canvasTopRight.x() - mainCanvas->fitParamsDialog->width() - 15;
        int posY = canvasTopRight.y() + 15;
        mainCanvas->fitParamsDialog->move(posX, posY);
    }

    mainCanvas->fitParamsDialog->show();

    // Ensure keyboard focus stays on the ROOT canvas unless the user is actively typing in a control
    QWidget *focused = QApplication::focusWidget();
    bool userEditing = (focused && mainCanvas->fitParamsDialog &&
                        (focused == mainCanvas->fitParamsDialog || mainCanvas->fitParamsDialog->isAncestorOf(focused)));
    if (!userEditing && mainCanvas->canvas) {
        mainCanvas->canvas->setFocus();
    }
}

//==============================================================================
// findPeaksWithTSpectrum
//==============================================================================
// Runs ROOT's TSpectrum deconvolution-based peak finding on the histogram.
// Operates on the current visible X-axis range, sorts peaks by channel in
// ascending order, and applies energy calibration if available.
//==============================================================================
std::vector<DetectedPeak> findPeaksWithTSpectrum(
    TH1F *hist,
    double sigma,
    double threshold,
    bool useVisibleRange
) {
    std::vector<DetectedPeak> result;
    if (!hist) return result;

    TAxis *xAxis = hist->GetXaxis();
    if (!xAxis) return result;

    const Int_t originalFirst = xAxis->GetFirst();
    const Int_t originalLast = xAxis->GetLast();
    const bool needsRangeReset = (!useVisibleRange && (originalFirst != 1 || originalLast != hist->GetNbinsX()));
    if (needsRangeReset) {
        xAxis->SetRange(1, hist->GetNbinsX());
    }

    // Clamp parameters to reasonable physics defaults
    if (sigma <= 0.0) sigma = 2.5;
    if (threshold <= 0.0) threshold = 0.05;
    if (threshold >= 1.0) threshold = 0.99;

    TSpectrum spectrum(200);
    // "goff" suppresses default ROOT TPolyMarker drawing
    Int_t nFound = spectrum.Search(hist, sigma, "goff", threshold);

    Double_t *xPeaks = spectrum.GetPositionX();
    Double_t *yPeaks = spectrum.GetPositionY();

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    bool isCalib = (trackHist && trackHist->IsCalibrated());

    // Pair positions and heights to sort by ascending channel
    std::vector<std::pair<double, double>> peakPairs;
    peakPairs.reserve(nFound);
    for (Int_t i = 0; i < nFound; ++i) {
        double ch = xPeaks[i];
        double h = (yPeaks != nullptr) ? yPeaks[i] : 0.0;
        if (h <= 0.0) {
            h = hist->GetBinContent(hist->FindBin(ch));
        }
        peakPairs.emplace_back(ch, h);
    }

    std::sort(peakPairs.begin(), peakPairs.end(),
              [](const std::pair<double, double> &a, const std::pair<double, double> &b) {
                  return a.first < b.first;
              });

    result.reserve(peakPairs.size());
    for (size_t i = 0; i < peakPairs.size(); ++i) {
        DetectedPeak dp;
        dp.index = static_cast<int>(i + 1);
        dp.channel = peakPairs[i].first;
        dp.height = peakPairs[i].second;
        dp.isCalibrated = isCalib;
        if (isCalib) {
            dp.energy = trackHist->ChannelToEnergy(dp.channel);
        } else {
            dp.energy = dp.channel;
        }
        result.push_back(dp);
    }

    if (needsRangeReset) {
        xAxis->SetRange(originalFirst, originalLast);
    }

    return result;
}

//==============================================================================
// showPeakSearchParamsDialog
//==============================================================================
// Displays an unfocused floating peak search dialog docked in the top-right corner.
// Provides interactive controls for sigma and threshold with real-time updates,
// a tabular view of detected peaks, and auto-dismiss on external interaction.
//==============================================================================
void showPeakSearchParamsDialog(
    QMainCanvas *mainCanvas,
    double sigma,
    double threshold,
    const std::vector<DetectedPeak> &peaks,
    double xMin,
    double xMax
) {
    if (!mainCanvas) return;

    QString upArrowPath, downArrowPath;
    ensureSpinArrowIcons(upArrowPath, downArrowPath);

    if (!mainCanvas->peakSearchParamsDialog) {
        mainCanvas->peakSearchParamsDialog = new QDialog(mainCanvas, Qt::Tool | Qt::WindowStaysOnTopHint);
        mainCanvas->peakSearchParamsDialog->setAttribute(Qt::WA_ShowWithoutActivating, true);
        mainCanvas->peakSearchParamsDialog->setFocusPolicy(Qt::NoFocus);
        mainCanvas->peakSearchParamsDialog->setStyleSheet(QString(
            "QDialog, QWidget {"
            "    background-color: #1e1e24;"
            "    color: #e0e0e0;"
            "}"
            "QDialog {"
            "    border: 1px solid #4f5b66;"
            "    border-radius: 6px;"
            "}"
            "QTextBrowser {"
            "    background-color: #181a1f;"
            "    color: #dcdfe4;"
            "    border: 1px solid #2c313a;"
            "    border-radius: 4px;"
            "    font-family: 'Consolas', 'DejaVu Sans Mono', 'Monaco', monospace;"
            "    font-size: 11px;"
            "    padding: 6px;"
            "}"
            "QGroupBox {"
            "    background-color: transparent;"
            "    border: 1px solid #3e4451;"
            "    border-radius: 5px;"
            "    margin-top: 6px;"
            "    padding: 8px 8px 8px 8px;"
            "    font-size: 11px;"
            "    font-weight: bold;"
            "    color: #61afef;"
            "}"
            "QGroupBox::title {"
            "    subcontrol-origin: margin;"
            "    subcontrol-position: top left;"
            "    left: 8px;"
            "    padding: 0 4px;"
            "}"
            "QDoubleSpinBox {"
            "    background-color: #14161a;"
            "    color: #e5c07b;"
            "    border: 1px solid #4f5b66;"
            "    border-radius: 4px;"
            "    padding: 3px 26px 3px 8px;"
            "    font-family: 'Consolas', 'DejaVu Sans Mono', 'Monaco', monospace;"
            "    font-size: 12px;"
            "    font-weight: bold;"
            "}"
            "QDoubleSpinBox:hover {"
            "    border: 1px solid #61afef;"
            "}"
            "QDoubleSpinBox:focus {"
            "    border: 1px solid #61afef;"
            "    background-color: #181a1f;"
            "}"
            "QDoubleSpinBox::up-button {"
            "    subcontrol-origin: border;"
            "    subcontrol-position: top right;"
            "    width: 22px;"
            "    height: 13px;"
            "    background-color: #2c313a;"
            "    border-left: 1px solid #4f5b66;"
            "    border-bottom: 1px solid #3e4451;"
            "    border-top-right-radius: 3px;"
            "}"
            "QDoubleSpinBox::up-button:hover {"
            "    background-color: #3e4451;"
            "}"
            "QDoubleSpinBox::up-arrow {"
            "    image: url(%1);"
            "    width: 9px;"
            "    height: 6px;"
            "}"
            "QDoubleSpinBox::down-button {"
            "    subcontrol-origin: border;"
            "    subcontrol-position: bottom right;"
            "    width: 22px;"
            "    height: 13px;"
            "    background-color: #2c313a;"
            "    border-left: 1px solid #4f5b66;"
            "    border-bottom-right-radius: 3px;"
            "}"
            "QDoubleSpinBox::down-button:hover {"
            "    background-color: #3e4451;"
            "}"
            "QDoubleSpinBox::down-arrow {"
            "    image: url(%2);"
            "    width: 9px;"
            "    height: 6px;"
            "}"
            "QLabel {"
            "    background-color: transparent;"
            "    color: #abb2bf;"
            "    font-size: 11px;"
            "}"
            "QPushButton {"
            "    background-color: #2c313a;"
            "    color: #dcdfe4;"
            "    border: 1px solid #4f5b66;"
            "    border-radius: 3px;"
            "    padding: 4px 10px;"
            "    font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #3e4451;"
            "}"
        ).arg(upArrowPath, downArrowPath));
        mainCanvas->peakSearchParamsDialog->setWindowTitle("Peak Search (TSpectrum)");
        mainCanvas->peakSearchParamsDialog->setFixedWidth(380);

        QVBoxLayout *layout = new QVBoxLayout(mainCanvas->peakSearchParamsDialog);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        // Parameters group box
        QGroupBox *grpParams = new QGroupBox("Search Parameters", mainCanvas->peakSearchParamsDialog);
        QGridLayout *paramGrid = new QGridLayout(grpParams);
        paramGrid->setContentsMargins(8, 10, 8, 8);
        paramGrid->setHorizontalSpacing(10);
        paramGrid->setVerticalSpacing(8);

        QLabel *lblSigma = new QLabel("Sigma (ch):", grpParams);
        lblSigma->setStyleSheet("font-weight: bold; color: #abb2bf;");

        mainCanvas->spinPeakSigma = new QDoubleSpinBox(grpParams);
        mainCanvas->spinPeakSigma->setRange(0.25, 50.0);
        mainCanvas->spinPeakSigma->setSingleStep(0.25); // 10% of default 2.50
        mainCanvas->spinPeakSigma->setDecimals(2);
        mainCanvas->spinPeakSigma->setValue(sigma);
        mainCanvas->spinPeakSigma->setFixedHeight(28);
        mainCanvas->spinPeakSigma->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        mainCanvas->spinPeakSigma->setFocusPolicy(Qt::NoFocus);
        mainCanvas->spinPeakSigma->setToolTip("Expected peak standard deviation in channels (10% step = 0.25 ch)");

        QLabel *lblThresh = new QLabel("Threshold:", grpParams);
        lblThresh->setStyleSheet("font-weight: bold; color: #abb2bf;");

        mainCanvas->spinPeakThreshold = new QDoubleSpinBox(grpParams);
        mainCanvas->spinPeakThreshold->setRange(0.001, 0.990);
        mainCanvas->spinPeakThreshold->setSingleStep(0.005); // 10% of default 0.050
        mainCanvas->spinPeakThreshold->setDecimals(3);
        mainCanvas->spinPeakThreshold->setValue(threshold);
        mainCanvas->spinPeakThreshold->setFixedHeight(28);
        mainCanvas->spinPeakThreshold->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        mainCanvas->spinPeakThreshold->setFocusPolicy(Qt::NoFocus);
        mainCanvas->spinPeakThreshold->setToolTip("Relative detection threshold (10% step = 0.005)");

        paramGrid->addWidget(lblSigma, 0, 0);
        paramGrid->addWidget(mainCanvas->spinPeakSigma, 0, 1);
        paramGrid->addWidget(lblThresh, 1, 0);
        paramGrid->addWidget(mainCanvas->spinPeakThreshold, 1, 1);
        layout->addWidget(grpParams);

        // Debounce timer for interactive parameter tuning
        mainCanvas->peakSearchDebounceTimer = new QTimer(mainCanvas->peakSearchParamsDialog);
        mainCanvas->peakSearchDebounceTimer->setSingleShot(true);
        QObject::connect(mainCanvas->peakSearchDebounceTimer, &QTimer::timeout, mainCanvas, [mainCanvas]() {
            if (mainCanvas->spinPeakSigma && mainCanvas->spinPeakThreshold) {
                mainCanvas->searchPeaksWithParams(
                    mainCanvas->spinPeakSigma->value(),
                    mainCanvas->spinPeakThreshold->value()
                );
            }
        });

        auto triggerSearchDebounce = [mainCanvas]() {
            if (mainCanvas->peakSearchDebounceTimer) {
                mainCanvas->peakSearchDebounceTimer->start(350);
            }
        };

        QObject::connect(mainCanvas->spinPeakSigma, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [triggerSearchDebounce](double) {
            triggerSearchDebounce();
        });
        QObject::connect(mainCanvas->spinPeakThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mainCanvas, [triggerSearchDebounce](double) {
            triggerSearchDebounce();
        });

        // Results text browser
        mainCanvas->peakSearchBrowser = new QTextBrowser(mainCanvas->peakSearchParamsDialog);
        mainCanvas->peakSearchBrowser->setReadOnly(true);
        mainCanvas->peakSearchBrowser->setFixedHeight(190);
        mainCanvas->peakSearchBrowser->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(mainCanvas->peakSearchBrowser);

        // Action buttons
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(6);

        QPushButton *btnLoadFit = new QPushButton("Use for Fit", mainCanvas->peakSearchParamsDialog);
        btnLoadFit->setFocusPolicy(Qt::NoFocus);
        btnLoadFit->setToolTip("Load detected peak centroids into multi-peak Gaussian fit markers");
        QObject::connect(btnLoadFit, &QPushButton::clicked, mainCanvas, &QMainCanvas::transferPeaksToGaussMarkers);

        QPushButton *btnClear = new QPushButton("Clear (Z+P)", mainCanvas->peakSearchParamsDialog);
        btnClear->setFocusPolicy(Qt::NoFocus);
        btnClear->setToolTip("Clear all peak search markers (Shortcut: Z+P)");
        QObject::connect(btnClear, &QPushButton::clicked, mainCanvas, &QMainCanvas::deletePeakMarkers);

        QPushButton *btnClose = new QPushButton("Close", mainCanvas->peakSearchParamsDialog);
        btnClose->setFocusPolicy(Qt::NoFocus);
        QObject::connect(btnClose, &QPushButton::clicked, mainCanvas->peakSearchParamsDialog, &QDialog::close);

        btnLayout->addWidget(btnLoadFit);
        btnLayout->addWidget(btnClear);
        btnLayout->addStretch();
        btnLayout->addWidget(btnClose);
        layout->addLayout(btnLayout);
    }

    // Update spinboxes without triggering recursive re-search
    if (mainCanvas->spinPeakSigma) {
        mainCanvas->spinPeakSigma->blockSignals(true);
        mainCanvas->spinPeakSigma->setValue(sigma);
        mainCanvas->spinPeakSigma->blockSignals(false);
    }
    if (mainCanvas->spinPeakThreshold) {
        mainCanvas->spinPeakThreshold->blockSignals(true);
        mainCanvas->spinPeakThreshold->setValue(threshold);
        mainCanvas->spinPeakThreshold->blockSignals(false);
    }

    // Build rich HTML table of detected peaks
    if (mainCanvas->peakSearchBrowser) {
        QString html;
        html += "<div style='font-family:monospace; font-size:11px; margin-bottom:6px;'>";
        html += QString("Range: <b>[%1, %2]</b> | Found: <b style='color:#98c379;'>%3 peaks</b></div>")
                    .arg(xMin, 0, 'f', 1)
                    .arg(xMax, 0, 'f', 1)
                    .arg(peaks.size());

        html += "<table width='100%' style='border-collapse:collapse; font-family:monospace; font-size:11px;'>";
        html += "<tr style='background-color:#21252b; color:#61afef;'>";
        html += "<th style='padding:2px 4px;' align='left'>#</th>";
        html += "<th style='padding:2px 4px;' align='right'>Channel</th>";
        html += "<th style='padding:2px 4px;' align='right'>Energy</th>";
        html += "<th style='padding:2px 4px;' align='right'>Counts</th>";
        html += "</tr>";

        for (size_t i = 0; i < peaks.size(); ++i) {
            const auto &p = peaks[i];
            QString bg = (i % 2 == 0) ? "#181a1f" : "#21252b";
            html += QString("<tr style='background-color:%1;'>").arg(bg);
            html += QString("<td style='padding:2px 4px;' align='left'>%1</td>").arg(p.index);
            html += QString("<td style='padding:2px 4px;' align='right'>%1</td>").arg(p.channel, 0, 'f', 1);
            if (p.isCalibrated) {
                html += QString("<td style='padding:2px 4px; color:#e5c07b;' align='right'>%1</td>").arg(p.energy, 0, 'f', 1);
            } else {
                html += "<td style='padding:2px 4px; color:#5c6370;' align='right'>-</td>";
            }
            html += QString("<td style='padding:2px 4px;' align='right'>%1</td>").arg(static_cast<long long>(p.height));
            html += "</tr>";
        }
        html += "</table>";

        mainCanvas->peakSearchBrowser->setHtml(html);
    }

    // Always dock in top-right corner of canvas
    if (mainCanvas->canvas) {
        QPoint canvasTopRight = mainCanvas->canvas->mapToGlobal(QPoint(mainCanvas->canvas->width(), 0));
        int posX = canvasTopRight.x() - mainCanvas->peakSearchParamsDialog->width() - 25;
        int posY = canvasTopRight.y() + 45;
        mainCanvas->peakSearchParamsDialog->move(posX, posY);
    }

    mainCanvas->peakSearchParamsDialog->show();

    // Keep focus on the ROOT canvas
    QWidget *focused = QApplication::focusWidget();
    bool userEditing = (focused && mainCanvas->peakSearchParamsDialog &&
                        (focused == mainCanvas->peakSearchParamsDialog || mainCanvas->peakSearchParamsDialog->isAncestorOf(focused)));
    if (!userEditing && mainCanvas->canvas) {
        mainCanvas->canvas->setFocus();
    }
}
