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

#include <TF1.h>
#include <TFormula.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TMatrixD.h>
#include <TMatrixDSym.h>
#include <TLine.h>
#include <TLatex.h>
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

    // Temporary histogram accumulating background intervals
    TracknHistogram tempHist("tempHist", "", 10240, 0, 10240);
    for (std::size_t i = 0; i < mainCanvas->background_markers.size() / 2; ++i) {
        const Int_t start  = mainCanvas->background_markers[2 * i];
        const Int_t finish = mainCanvas->background_markers[2 * i + 1];
        for (Int_t j = start; j <= finish; ++j) {
            tempHist.AddBinContent(j, activeHist->GetBinContent(j));
        }

        const Double_t localMin = findMinValueInInterval(activeHist, start, finish);
        if (localMin < minimum) {
            minimum = localMin;
        }
    }

    mainCanvas->background = new TFormula("background", "[0]*x+[1]");
    mainCanvas->backgroundFunction = new TF1("backgroundFunction", "background", 0, 10240);
    mainCanvas->backgroundFunction->SetParameter(0, 0.0);
    mainCanvas->backgroundFunction->SetParameter(1, minimum);

    TFitResultPtr fitResult = tempHist.Fit(mainCanvas->backgroundFunction, "QMSW", "same");

    mainCanvas->backgroundA0 = mainCanvas->backgroundFunction->GetParameter(1);
    mainCanvas->backgroundA1 = mainCanvas->backgroundFunction->GetParameter(0);

    const Double_t r0 = mainCanvas->range_markers[0];
    const Double_t r1 = mainCanvas->range_markers[1];
    mainCanvas->backgroundIntegral = mainCanvas->backgroundFunction->Integral(r0, r1);
    mainCanvas->backgroundIntegralError = mainCanvas->backgroundFunction->IntegralError(
        r0, r1, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());

    delete mainCanvas->backgroundCovarianceMatrix;
    mainCanvas->backgroundCovarianceMatrix = new TMatrixD(fitResult->GetCovarianceMatrix());

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
// runAutoFit
//==============================================================================
// Performs an automated single-peak Gaussian fit around the clicked channel.
//
// Model function:
//   y(x) = [0] * exp( -(x - [1])^2 / (2 * [2]) ) + [3] * x + [4]
//
// Adapts fit range to +/- 3*FWHM if initial peak is broader than (41 / 6) bins.
// Outputs results to CommandPrompt console and registers centroid into puncte_calib2p.
//==============================================================================
void runAutoFit(QMainCanvas *mainCanvas, int x, int y)
{
    if (!mainCanvas) return;

    int binX = mainCanvas->getBinFromClick(x, y);
    TH1F *hist = mainCanvas->HijF[mainCanvas->SelectedElement_i][mainCanvas->SelectedElement_j];
    if (!hist) return;

    int binC = hist->GetBinContent(binX);
    Double_t gaussianHeight = 0.0, gaussianCenter = 0.0, gaussianSigma = 0.0;
    Double_t bkgSlope = 0.0, bkg0 = 0.0, gaussianFWHM = 0.0;
    Double_t gaussianCenterError = 0.0, gaussianIntegral = 0.0;
    Double_t gaussianIntegralError = 0.0, gaussianFWHMError = 0.0;

    mainCanvas->gaussianWithBackground = new TFormula(
        "gaussianWithBackground", "[0]*exp(-(x-[1])^2/(2*[2]))+[3]*x+[4]");
    mainCanvas->gaussianWithBackgroundFunction = new TF1(
        "gaussianWithBackgroundFunction", "gaussianWithBackground", binX - 20, binX + 20);

    mainCanvas->background = new TFormula("background", "[0]*x+[1]");
    mainCanvas->backgroundFunction = new TF1("backgroundFunction", "background", binX - 20, binX + 20);

    mainCanvas->gaussianCenterMarkerText = new TLatex();

    // Initial parameter estimates
    mainCanvas->gaussianWithBackgroundFunction->SetParameter(0, binC);
    mainCanvas->gaussianWithBackgroundFunction->SetParameter(1, binX);
    mainCanvas->gaussianWithBackgroundFunction->SetParameter(2, 4.0);
    mainCanvas->gaussianWithBackgroundFunction->SetParameter(3, 0.0);
    mainCanvas->gaussianWithBackgroundFunction->SetParameter(
        4, findMinValueInInterval(hist, binX - 20, binX + 20));

    // Fit with Minuit
    TFitResultPtr fitResult = hist->Fit(mainCanvas->gaussianWithBackgroundFunction, "QMRS", "same");

    bkgSlope = mainCanvas->gaussianWithBackgroundFunction->GetParameter(3);
    bkg0     = mainCanvas->gaussianWithBackgroundFunction->GetParameter(4);
    mainCanvas->backgroundFunction->FixParameter(0, bkgSlope);
    mainCanvas->backgroundFunction->FixParameter(1, bkg0);

    gaussianSigma = mainCanvas->gaussianWithBackgroundFunction->GetParameter(2);
    gaussianFWHM  = gaussianSigma * 2.35482;
    gaussianIntegral = mainCanvas->gaussianWithBackgroundFunction->Integral(binX - 20, binX + 20)
                     - mainCanvas->backgroundFunction->Integral(binX - 20, binX + 20);
    if (fitResult.Get()) {
        gaussianIntegralError = mainCanvas->gaussianWithBackgroundFunction->IntegralError(
            binX - 20, binX + 20, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
    }

    // Adaptive refit for broad peaks
    if (gaussianFWHM > (41.0 / 6.0)) {
        const Double_t fitMin = binX - gaussianFWHM * 3.0;
        const Double_t fitMax = binX + gaussianFWHM * 3.0;
        mainCanvas->gaussianWithBackgroundFunction->SetRange(fitMin, fitMax);
        mainCanvas->backgroundFunction->SetRange(fitMin, fitMax);

        fitResult = hist->Fit(mainCanvas->gaussianWithBackgroundFunction, "QMRS", "");

        gaussianSigma = mainCanvas->gaussianWithBackgroundFunction->GetParameter(2);
        gaussianFWHM  = gaussianSigma * 2.35482;
        gaussianIntegral = mainCanvas->gaussianWithBackgroundFunction->Integral(fitMin, fitMax)
                         - mainCanvas->backgroundFunction->Integral(fitMin, fitMax);
        if (fitResult.Get()) {
            gaussianIntegralError = mainCanvas->gaussianWithBackgroundFunction->IntegralError(
                fitMin, fitMax, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
        }
    }

    gaussianHeight      = mainCanvas->gaussianWithBackgroundFunction->GetParameter(0);
    gaussianCenter      = mainCanvas->gaussianWithBackgroundFunction->GetParameter(1);
    gaussianCenterError = mainCanvas->gaussianWithBackgroundFunction->GetParError(1);
    gaussianFWHMError   = mainCanvas->gaussianWithBackgroundFunction->GetParError(2) * 2.35482;

    // Display peak centroid label on canvas
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f", gaussianCenter);
    mainCanvas->gaussianCenterMarkerText->DrawLatex(gaussianCenter, gaussianHeight, buffer);
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

    mainCanvas->puncte_calib2p.push_back(gaussianCenter);

    // Report table output
    QString headerRow = QString("%1%2%3%4%5")
        .arg("Peak#",    -10, QChar(' '))
        .arg("Channel",  -10, QChar(' '))
        .arg("Energy",   -15, QChar(' '))
        .arg("Area",     -25, QChar(' '))
        .arg("Width",    -10, QChar(' '));
    CommandPrompt::getInstance()->appendPlainText(headerRow);

    std::cout << std::left
              << std::setw(10) << "Peak#"
              << std::setw(10) << "Channel"
              << std::setw(15) << "Energy"
              << std::setw(25) << "Area"
              << std::setw(10) << "Width" << std::endl;

    QString numberStr           = QString("%1").arg("1", -10, QChar(' '));
    QString gaussianCenterStr   = QString("%1").arg(gaussianCenter, -10, 'f', 2, QChar(' '));
    QString energyStr           = QString("%1(%2)").arg(gaussianCenter, 0, 'f', 2).arg(qCeil(gaussianCenterError * 100));
    QString gaussianIntegralStr = QString("%1(%2)").arg(gaussianIntegral, 0, 'f', 0).arg(qRound(gaussianIntegralError));
    QString gaussianFWHMStr     = QString("%1(%2)").arg(gaussianFWHM, 0, 'f', 2).arg(qCeil(gaussianFWHMError * 100));

    QString dataRow = QString("%1%2%3%4%5")
        .arg(numberStr)
        .arg(gaussianCenterStr)
        .arg(energyStr,           -15, QChar(' '))
        .arg(gaussianIntegralStr, -25, QChar(' '))
        .arg(gaussianFWHMStr,     -10, QChar(' '));

    CommandPrompt::getInstance()->appendPlainText(dataRow + "\n");

    std::cout << std::setw(10) << "1"
              << std::setw(10) << std::fixed << std::setprecision(2) << gaussianCenter
              << std::setw(15) << energyStr.toStdString()
              << std::setw(25) << gaussianIntegralStr.toStdString()
              << std::setw(10) << gaussianFWHMStr.toStdString() << std::endl;

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
// runMultiPeakFit
//==============================================================================
// Performs multi-peak Gaussian fitting with background over the designated range.
//
// Steps:
// 1. Validates and sanitizes background, range, and Gaussian centroid markers.
// 2. Fits linear background over background markers (fitBackgroundHelper).
// 3. Assembles composite multi-Gaussian function:
//      F(x) = background + sum_i( A_i * exp( -(x - mu_i)^2 / (2*sigma_i^2) ) )
// 4. Sets initial parameter estimates from marked channels.
// 5. Fits with Minuit (Q M R S), with adaptive strategy/tolerance retries if needed.
// 6. Extracts individual peak integrals and covariance sub-matrices.
// 7. Combines peak error with background uncertainty:
//      sigma_total = sqrt( sigma_fit^2 + sigma_bkg^2 )
// 8. Outputs monospace analysis table to CommandPrompt and stdout.
//==============================================================================
void runMultiPeakFit(QMainCanvas *mainCanvas)
{
    if (!mainCanvas) return;

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

    const Double_t r0 = mainCanvas->range_markers[0];
    const Double_t r1 = mainCanvas->range_markers[1];
    const Double_t maxValue = findMaxValueInInterval(activeHist, r0, r1);

    // Fit linear background over background regions
    fitBackgroundHelper(mainCanvas);

    // Construct composite function with background and N Gaussians
    TF1 *fullFunction = new TF1("fullFunction", "backgroundFunction", r0, r1);
    for (std::size_t i = 0; i < mainCanvas->gauss_markers.size(); ++i) {
        fullFunction = new TF1("fullFunction", "fullFunction+gaussian", r0, r1);
    }

    // Fix background parameters from linear background fit
    fullFunction->FixParameter(0, mainCanvas->backgroundA1);
    fullFunction->FixParameter(1, mainCanvas->backgroundA0);

    // Initialize Gaussian parameters (amplitude, centroid, sigma)
    for (std::size_t i = 0; i < mainCanvas->gauss_markers.size(); ++i) {
        const Int_t peakChannel = static_cast<Int_t>(mainCanvas->gauss_markers[i] - 1);
        fullFunction->SetParameter(2 + i * 3, activeHist->GetBinContent(peakChannel));
        fullFunction->SetParLimits(2 + i * 3, 0.0, maxValue * 1.1);
        fullFunction->SetParameter(3 + i * 3, peakChannel);
        fullFunction->SetParLimits(3 + i * 3, r0, r1);
        fullFunction->SetParameter(4 + i * 3, 3.0);
        fullFunction->SetParLimits(4 + i * 3, 0.4, std::abs(r1 - r0) * 4.0);
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

    // Print table header
    std::cout << std::left
              << std::setw(10) << "Peak#"
              << std::setw(10) << "Channel"
              << std::setw(15) << "Energy"
              << std::setw(25) << "Area"
              << std::setw(10) << "Width" << std::endl;

    QString headerRow = QString("%1%2%3%4%5")
        .arg("Peak#",   -10, QChar(' '))
        .arg("Channel", -10, QChar(' '))
        .arg("Energy",  -15, QChar(' '))
        .arg("Area",    -25, QChar(' '))
        .arg("Width",   -10, QChar(' '));
    CommandPrompt::getInstance()->appendPlainText(headerRow);

    // Calculate individual peak integrals, uncertainties, and print data rows
    for (std::size_t i = 0; i < mainCanvas->gauss_markers.size(); ++i) {
        TF1 tempGaussFunction("tempGaussFunction", "gaussian", r0, r1);
        tempGaussFunction.SetParameter(0, fullFunction->GetParameter(2 + i * 3));
        tempGaussFunction.SetParError(0, fullFunction->GetParError(2 + i * 3));
        tempGaussFunction.SetParameter(1, fullFunction->GetParameter(3 + i * 3));
        tempGaussFunction.SetParError(1, fullFunction->GetParError(3 + i * 3));
        tempGaussFunction.SetParameter(2, fullFunction->GetParameter(4 + i * 3));
        tempGaussFunction.SetParError(2, fullFunction->GetParError(4 + i * 3));

        Double_t fitIntegralError = 0.0;
        if (fitResult.Get()) {
            TMatrixDSym subCov = fitResult->GetCovarianceMatrix().GetSub(
                2 + i * 3, 4 + i * 3, 2 + i * 3, 4 + i * 3);
            fitIntegralError = tempGaussFunction.IntegralError(
                r0, r1, tempGaussFunction.GetParameters(), subCov.GetMatrixArray());
        }

        // Combined uncertainty: Gaussian peak integral error + background uncertainty
        const Double_t combinedAreaError = std::sqrt(
            fitIntegralError * fitIntegralError +
            mainCanvas->backgroundIntegralError * mainCanvas->backgroundIntegralError);

        const Double_t peakCenter = fullFunction->GetParameter(3 + i * 3);
        const Double_t centerErr   = fullFunction->GetParError(3 + i * 3);
        const Double_t peakArea   = tempGaussFunction.Integral(r0, r1);
        const Double_t peakSigma  = fullFunction->GetParameter(4 + i * 3);
        const Double_t peakFWHM   = peakSigma * 2.35482;
        const Double_t fwhmErr    = fullFunction->GetParError(4 + i * 3) * 2.35482;

        QString numberStr         = QString("%1").arg(i + 1, -10, QChar(' '));
        QString centerStr         = QString("%1").arg(peakCenter, -10, 'f', 2, QChar(' '));
        QString energyStr         = QString("%1(%2)").arg(peakCenter, 0, 'f', 2).arg(qCeil(centerErr * 100));
        QString areaStr           = QString("%1(%2)").arg(peakArea, 0, 'f', 0).arg(qRound(combinedAreaError));
        QString fwhmStr           = QString("%1(%2)").arg(peakFWHM, 0, 'f', 2).arg(qCeil(fwhmErr * 100));

        QString dataRow = QString("%1%2%3%4%5")
            .arg(numberStr)
            .arg(centerStr)
            .arg(energyStr, -15, QChar(' '))
            .arg(areaStr,   -25, QChar(' '))
            .arg(fwhmStr,   -10, QChar(' '));

        CommandPrompt::getInstance()->appendPlainText(dataRow);

        std::cout << std::setw(10) << (i + 1)
                  << std::setw(10) << std::fixed << std::setprecision(2) << peakCenter
                  << std::setw(15) << energyStr.toStdString()
                  << std::setw(25) << areaStr.toStdString()
                  << std::setw(10) << fwhmStr.toStdString() << std::endl;
    }

    CommandPrompt::getInstance()->appendPlainText("");

    mainCanvas->canvas->getCanvas()->Modified();
    mainCanvas->canvas->getCanvas()->Update();
}
