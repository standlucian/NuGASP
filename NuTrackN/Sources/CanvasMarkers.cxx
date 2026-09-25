#include "IntegralDialog.h"
#include "canvas.h"
#include "Design.h"
#include "Integral.h"
#include "PeakFit.h"
#include "tracknhistogram.h"
#include "MatrixReader.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TLine.h>
#include <TBox.h>
#include <TLatex.h>
#include <TVirtualPad.h>
#include <TColor.h>
#include <QTimer>

static inline Color_t toMarkerColor(const QColor &c) {
    return TColor::GetColor(c.name().toUtf8().constData());
}

#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <QInputDialog>

//==============================================================================
// QMainCanvas::addSpaceBarMarker
//==============================================================================
// Adds a vertical spacebar marker (cyan TLine) at the channel coordinate clicked
// by the user. Pairs of spacebar markers define the boundaries of the region of
// interest to be zoomed when the user subsequently presses 'E', and also serve
// as reference markers for two-point energy calibration (Cal2P).
//==============================================================================
void QMainCanvas::addSpaceBarMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);

    // Record the channel in spacebar, zoom, and calibration marker collections
    spacebar_markers.push_back(static_cast<Double_t>(binX));
    zoom_markers.push_back(binX);
    puncte_calib2p.push_back(static_cast<Float_t>(binX));

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    const Double_t yMax = hist ? (hist->GetMaximum() * 1.05) : 100.0;

    // Create a vertical marker line and draw it over the spectrum
    TLine *spacebarLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    spacebarLine->SetLineColor(toMarkerColor(Design::getZoomMarkerColor()));
    spacebarLine->SetLineWidth(2);
    spacebarLine->Draw("same");

    // Register graphical object so it can be cleared on reset or zoom
    listOfObjectsDrawnOnScreen.Add(spacebarLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::areaFunction
//==============================================================================
// Computes gross peak area without background subtraction (triggered by 'C + I'
// shortcut). Delegates calculation to integral_function() in Integral.h with
// an empty background marker set, and overlays peak index labels on the canvas.
//==============================================================================
void QMainCanvas::areaFunction()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    if (integral_markers.empty()) {
        const QString msg = "Place markers with 'I' for the integral to be calculated.\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
        return;
    }

    std::vector<Int_t> placeholder_background_markers;
    std::vector<IntegratedPeak> peaks;
    integral_function(hist,
                      integral_markers,
                      placeholder_background_markers,
                      slope,
                      addition,
                      &peaks);

    // Render peak index labels on the canvas above peak centroids
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    for (const auto &peak : peaks) {
        Int_t bin = hist->FindBin(peak.centroid);
        Double_t peakY = hist->GetBinContent(bin);
        if (peakY <= 0.0) peakY = hist->GetMaximum() * 0.5;
        Double_t labelY = peakY * 1.05;

        char labelBuf[32];
        if (peak.isCalibrated) {
            snprintf(labelBuf, sizeof(labelBuf), "[%d] %.1f", peak.index, peak.energy);
        } else {
            snprintf(labelBuf, sizeof(labelBuf), "[%d]", peak.index);
        }

        TLatex *lbl = new TLatex(peak.centroid, labelY, labelBuf);
        lbl->SetName(Form("IntPeakLabel_%d", peak.index));
        lbl->SetTextFont(Design::getRootGraphFont(3));
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    if (m_isAreaLoggingEnabled && !peaks.empty()) {
        writeAreaLogHeader(false);
        for (const auto &peak : peaks) {
            double dispCentroid = peak.isCalibrated ? peak.energy : peak.centroid;
            double dispWidth = peak.isCalibrated ? peak.energyFwhm : peak.fwhm;
            writeAreaLogData(dispCentroid, dispWidth, peak.grossArea, peak.area, peak.bgArea, peak.areaError);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::areaFunctionWithBackground
//==============================================================================
// Computes net peak area (triggered by 'Int' UI button or 'C + J' shortcut).
// If background markers exist, fits linear background and overlays baseline;
// if no background markers exist, gracefully falls back to gross peak integration.
// Labels peak index above peak centroid on the canvas.
//==============================================================================
void QMainCanvas::areaFunctionWithBackground(bool openDialog)
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    if (integral_markers.empty()) {
        const QString msg = "Place markers with 'I' for the integral to be calculated.\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
        return;
    }

    std::vector<IntegratedPeak> peaks;
    integral_function(hist,
                      integral_markers,
                      background_markers,
                      slope,
                      addition,
                      &peaks);

    // If background markers exist, draw background baseline
    if (!background_markers.empty() && background_markers.size() >= 2) {
        const Double_t xStart = background_markers.front() - 0.5;
        const Double_t xEnd   = background_markers.back() - 0.5;
        TLine *backgroundLine = new TLine(xStart, slope * xStart + addition,
                                          xEnd,   slope * xEnd   + addition);
        backgroundLine->SetLineColor(toMarkerColor(Design::getBackgroundMarkerColor()));
        backgroundLine->SetLineWidth(2);
        backgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundLine);
    }

    // Render peak index labels on the canvas above peak centroids
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    for (const auto &peak : peaks) {
        Int_t bin = hist->FindBin(peak.centroid);
        Double_t peakY = hist->GetBinContent(bin);
        if (peakY <= 0.0) peakY = hist->GetMaximum() * 0.5;
        Double_t labelY = peakY * 1.05;

        char labelBuf[32];
        if (peak.isCalibrated) {
            snprintf(labelBuf, sizeof(labelBuf), "[%d] %.1f", peak.index, peak.energy);
        } else {
            snprintf(labelBuf, sizeof(labelBuf), "[%d]", peak.index);
        }

        TLatex *lbl = new TLatex(peak.centroid, labelY, labelBuf);
        lbl->SetName(Form("IntPeakLabel_%d", peak.index));
        lbl->SetTextFont(Design::getRootGraphFont(3));
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    if (m_isAreaLoggingEnabled && !peaks.empty()) {
        writeAreaLogHeader(false);
        for (const auto &peak : peaks) {
            double dispCentroid = peak.isCalibrated ? peak.energy : peak.centroid;
            double dispWidth = peak.isCalibrated ? peak.energyFwhm : peak.fwhm;
            writeAreaLogData(dispCentroid, dispWidth, peak.grossArea, peak.area, peak.bgArea, peak.areaError);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    // Show the interactive dialog to allow tweaking
    if (openDialog) {
        openIntegralDialog();
    }
}

//==============================================================================
// QMainCanvas::autoFit
//==============================================================================
// Delegates automated single-peak Gaussian fitting to the PeakFit module.
//==============================================================================
void QMainCanvas::autoFit(int x, int y)
{
    runAutoFit(this, x, y);
}

//==============================================================================
// QMainCanvas::findMinValueInInterval
//==============================================================================
// Delegates interval minimum search to PeakFit module.
//==============================================================================
Double_t QMainCanvas::findMinValueInInterval(int intervalStart, int intervalFinish)
{
    return ::findMinValueInInterval(HijF[SelectedElement_i][SelectedElement_j],
                                    intervalStart, intervalFinish);
}

//==============================================================================
// QMainCanvas::findMaxValueInInterval
//==============================================================================
// Delegates interval maximum search to PeakFit module.
//==============================================================================
Double_t QMainCanvas::findMaxValueInInterval(int intervalStart, int intervalFinish)
{
    return ::findMaxValueInInterval(HijF[SelectedElement_i][SelectedElement_j],
                                    intervalStart, intervalFinish);
}

//==============================================================================
// QMainCanvas::addBackgroundMarker
//==============================================================================
// Drops a blue vertical background marker at the clicked channel coordinate.
// Background markers are placed in pairs [left, right]. When an even marker
// completes a pair, a blue baseline and a hatched shaded box (fill style 3545)
// are drawn across the background estimation interval.
//==============================================================================
void QMainCanvas::addBackgroundMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    background_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    Color_t bgCol = toMarkerColor(Design::getBackgroundMarkerColor());

    // When placing the second marker of a pair, ensure previous boundary is redrawn
    if (background_markers.size() % 2 == 0 && !background_markers.empty()) {
        const std::size_t i = background_markers.size();
        TLine *backgroundLineSecond = new TLine(background_markers[i - 2] - 0.5, 0.0,
                                                background_markers[i - 2] - 0.5, yMax);
        backgroundLineSecond->SetLineColor(bgCol);
        backgroundLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        backgroundLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(backgroundLineSecond);
    }

    // Draw vertical boundary line at current clicked position
    TLine *backgroundLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    backgroundLine->SetLineColor(bgCol);
    backgroundLine->SetLineWidth(2);
    backgroundLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(backgroundLine);

    // When completing a pair, draw baseline and hatched region
    if (background_markers.size() % 2 == 0) {
        const Int_t leftBin = background_markers[background_markers.size() - 2];
        TLine *bottomBackgroundLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
        bottomBackgroundLine->SetLineColor(bgCol);
        bottomBackgroundLine->SetLineWidth(2);
        bottomBackgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

        TBox *backgroundArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
        backgroundArea->SetFillColor(bgCol);
        backgroundArea->SetFillStyle(3545);
        backgroundArea->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundArea);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::addIntegralMarker
//==============================================================================
// Drops a yellow vertical marker line at the clicked channel coordinate marking
// peak integration boundaries.
//==============================================================================
void QMainCanvas::addIntegralMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    integral_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    Color_t intCol = toMarkerColor(Design::getIntegralMarkerColor());

    // If second marker of pair, ensure the first is properly drawn
    if (integral_markers.size() % 2 == 0 && !integral_markers.empty()) {
        const std::size_t i = integral_markers.size();
        TLine *integralLineSecond = new TLine(integral_markers[i - 2] - 0.5, 0.0,
                                              integral_markers[i - 2] - 0.5, yMax);
        integralLineSecond->SetLineColor(intCol);
        integralLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        integralLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(integralLineSecond);
    }

    TLine *integralLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    integralLine->SetLineColor(intCol);
    integralLine->SetLineWidth(2);
    integralLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(integralLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::clearTheScreen
//==============================================================================
// Clears all visual marker lines, shaded boxes, and fit curves from the screen,
// freeing the graphical objects from memory without modifying the histogram data.
// Triggered by the '=' key shortcut.
//==============================================================================
void QMainCanvas::clearTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Free all dynamically allocated graphical primitives drawn on the canvas
    clearDrawnObjects();

    if (fitParamsDialog && fitParamsDialog->isVisible()) {
        fitParamsDialog->hide();
    }
    if (bkgDebounceTimer && bkgDebounceTimer->isActive()) {
        bkgDebounceTimer->stop();
    }
    m_bkgFixed = false;
    m_peakFixedStates.clear();

    // Clear fit markers and peak search markers (matching Xtrackn CleanPlot)
    autoFitMarkers[SelectedElement_i][SelectedElement_j].clear();
    gaussCenters[SelectedElement_i][SelectedElement_j].clear();
    gaussCentersHeight[SelectedElement_i][SelectedElement_j].clear();
    renderPeakLabels(SelectedElement_i, SelectedElement_j);

    if (peakSearchParamsDialog && peakSearchParamsDialog->isVisible()) {
        peakSearchParamsDialog->hide();
    }
    peakSearchCenters[SelectedElement_i][SelectedElement_j].clear();
    peakSearchHeights[SelectedElement_i][SelectedElement_j].clear();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (HijF[SelectedElement_i][SelectedElement_j]) {
        // Clear overlaid comparison spectra and reset to base histogram
        for (auto *h : HijC[SelectedElement_i][SelectedElement_j]) {
            delete h;
        }
        HijC[SelectedElement_i][SelectedElement_j].clear();
        HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[0]);
        TH1F *baseClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        baseClone->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(baseClone);

        // Adjust Y-axis scale to clean base spectrum counts
        adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
        HijF[SelectedElement_i][SelectedElement_j]->Draw();
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();

    CommandPrompt::getInstance()->appendPlainText("Display reset to clean base spectrum (=).\n");
}

//==============================================================================
// QMainCanvas::deleteBackgroundMarkers
//==============================================================================
// Clears all stored background marker positions (shortcut: 'Z + B').
//==============================================================================
void QMainCanvas::deleteBackgroundMarkers()
{
    background_markers.clear();
}

//==============================================================================
// QMainCanvas::deleteIntegralMarkers
//==============================================================================
// Clears all stored peak integration marker positions (shortcut: 'Z + I').
//==============================================================================
void QMainCanvas::deleteIntegralMarkers()
{
    integral_markers.clear();
}

//==============================================================================
// QMainCanvas::deleteAllMarkers
//==============================================================================
// Clears all active markers of all types from memory (shortcut: 'Z + A').
//==============================================================================
void QMainCanvas::deleteAllMarkers()
{
    deleteBackgroundMarkers();
    deleteIntegralMarkers();
    deleteRangeMarkers();
    deleteGaussMarkers();
    deleteGateMarkers();
}

//==============================================================================
// QMainCanvas::showBackgroundMarkers
//==============================================================================
// Re-renders all stored background markers, baselines, and shaded regions on
// the canvas (shortcut: 'M + B').
//==============================================================================
void QMainCanvas::showBackgroundMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    Color_t bgCol = toMarkerColor(Design::getBackgroundMarkerColor());
    for (std::size_t i = 0; i < background_markers.size(); ++i) {
        TLine *backgroundLine = new TLine(background_markers[i] - 0.5, 0.0,
                                          background_markers[i] - 0.5, yMax);
        backgroundLine->SetLineColor(bgCol);
        backgroundLine->SetLineWidth(2);
        backgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundLine);

        // When completing a pair, redraw baseline and hatched box
        if (i % 2 == 1) {
            TLine *bottomBackgroundLine = new TLine(background_markers[i - 1] - 0.5, 0.0,
                                                    background_markers[i] - 0.5, 0.0);
            bottomBackgroundLine->SetLineColor(bgCol);
            bottomBackgroundLine->SetLineWidth(2);
            bottomBackgroundLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

            TBox *backgroundArea = new TBox(background_markers[i - 1] - 0.5, 0.0,
                                            background_markers[i] - 0.5, yMax);
            backgroundArea->SetFillColor(bgCol);
            backgroundArea->SetFillStyle(3545);
            backgroundArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(backgroundArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showIntegralMarkers
//==============================================================================
// Re-renders all stored yellow peak integral markers on the canvas (shortcut: 'M + I').
//==============================================================================
void QMainCanvas::showIntegralMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    Color_t intCol = toMarkerColor(Design::getIntegralMarkerColor());
    for (std::size_t i = 0; i < integral_markers.size(); ++i) {
        TLine *integralLine = new TLine(integral_markers[i] - 0.5, 0.0,
                                        integral_markers[i] - 0.5, yMax);
        integralLine->SetLineColor(intCol);
        integralLine->SetLineWidth(2);
        integralLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(integralLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showAllMarkers
//==============================================================================
// Redraws all stored markers (background, integral, range, and Gauss) on the
// canvas (shortcut: 'M + A').
//==============================================================================
void QMainCanvas::showAllMarkers()
{
    showBackgroundMarkers();
    showIntegralMarkers();
    showRangeMarkers();
    showGaussMarkers();
    showGateMarkers();
}

//==============================================================================
// QMainCanvas::addRangeMarker
//==============================================================================
// Drops a yellow vertical range boundary marker at the clicked channel. Exactly two
// range markers define the region for multi-peak Gaussian fitting.
//==============================================================================
void QMainCanvas::addRangeMarker(Int_t x, Int_t y)
{
    if (range_markers.size() < 2) {
        int binX = getBinFromClick(x, y);
        range_markers.push_back(binX);

        TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
        if (!hist) return;

        const Double_t yMax = hist->GetMaximum() * 1.05;

        Color_t rangeCol = toMarkerColor(Design::getRangeMarkerColor());

        // When placing the second marker of a pair, ensure the first is drawn
        if (range_markers.size() % 2 == 0 && !range_markers.empty()) {
            const std::size_t i = range_markers.size();
            TLine *rangeLineSecond = new TLine(range_markers[i - 2] - 0.5, 0.0,
                                               range_markers[i - 2] - 0.5, yMax);
            rangeLineSecond->SetLineColor(rangeCol);
            rangeLineSecond->SetLineWidth(2);
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            rangeLineSecond->Draw();
            listOfObjectsDrawnOnScreen.Add(rangeLineSecond);
        }

        TLine *rangeLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
        rangeLine->SetLineColor(rangeCol);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        // If pair is complete, draw baseline and shaded region
        if (range_markers.size() % 2 == 0) {
            const Int_t leftBin = range_markers[range_markers.size() - 2];
            TLine *bottomRangeLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
            bottomRangeLine->SetLineColor(rangeCol);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
            rangeArea->SetFillColor(rangeCol);
            rangeArea->SetFillStyle(3545);
            rangeArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(rangeArea);
        }

        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    } else {
        // Warn user if attempting to place more than 2 range markers
        printf("\a");
        CommandPrompt::getInstance()->appendPlainText("Fitting range is already defined by two markers (use Z+R to clear).\n");
    }
}

//==============================================================================
// QMainCanvas::deleteRangeMarkers
//==============================================================================
// Clears stored fit range boundary markers (shortcut: 'Z + R').
//==============================================================================
void QMainCanvas::deleteRangeMarkers()
{
    range_markers.clear();
}

//==============================================================================
// QMainCanvas::showRangeMarkers
//==============================================================================
// Re-renders all stored yellow fit range markers and shaded intervals on the canvas
// (shortcut: 'M + R').
//==============================================================================
void QMainCanvas::showRangeMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    Color_t rangeCol = toMarkerColor(Design::getRangeMarkerColor());
    for (std::size_t i = 0; i < range_markers.size(); ++i) {
        TLine *rangeLine = new TLine(range_markers[i] - 0.5, 0.0,
                                     range_markers[i] - 0.5, yMax);
        rangeLine->SetLineColor(rangeCol);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        if (i % 2 == 1) {
            TLine *bottomRangeLine = new TLine(range_markers[i - 1] - 0.5, 0.0,
                                               range_markers[i] - 0.5, 0.0);
            bottomRangeLine->SetLineColor(rangeCol);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(range_markers[i - 1] - 0.5, 0.0,
                                       range_markers[i] - 0.5, yMax);
            rangeArea->SetFillColor(rangeCol);
            rangeArea->SetFillStyle(3545);
            rangeArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(rangeArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::addGaussMarker
//==============================================================================
// Drops a pink vertical marker line at the clicked channel coordinate marking
// an initial peak centroid estimate for multi-peak Gaussian fitting.
//==============================================================================
void QMainCanvas::addGaussMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    gauss_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    TLine *gaussLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    gaussLine->SetLineColor(toMarkerColor(Design::getGaussMarkerColor()));
    gaussLine->SetLineWidth(2);
    gaussLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(gaussLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::deleteGaussMarkers
//==============================================================================
// Clears all stored Gaussian peak centroid estimate markers (shortcut: 'Z + G').
//==============================================================================
void QMainCanvas::deleteGaussMarkers()
{
    gauss_markers.clear();
}

//==============================================================================
// QMainCanvas::showGaussMarkers
//==============================================================================
// Re-renders all stored Gaussian centroid estimate markers on the canvas
// (shortcut: 'M + G').
//==============================================================================
void QMainCanvas::showGaussMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    Color_t gaussCol = toMarkerColor(Design::getGaussMarkerColor());
    for (std::size_t i = 0; i < gauss_markers.size(); ++i) {
        TLine *gaussLine = new TLine(gauss_markers[i] - 0.5, 0.0,
                                     gauss_markers[i] - 0.5, yMax);
        gaussLine->SetLineColor(gaussCol);
        gaussLine->SetLineWidth(2);
        gaussLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gaussLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::addGateMarker
//==============================================================================
// Drops a magenta vertical coincidence gate marker at clicked channel (shortcut: 'W').
// Even markers complete a coincidence gate window [minB, maxB], drawing a baseline
// and shaded hatched area (fill style 3354).
//==============================================================================
void QMainCanvas::addGateMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    gate_markers.push_back(static_cast<Double_t>(binX));

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    Color_t gateCol = toMarkerColor(Design::getGateMarkerColor());

    // When placing the second marker of a pair, ensure previous boundary is redrawn
    if (gate_markers.size() % 2 == 0 && !gate_markers.empty()) {
        const std::size_t i = gate_markers.size();
        TLine *gateLineSecond = new TLine(gate_markers[i - 2] - 0.5, 0.0,
                                          gate_markers[i - 2] - 0.5, yMax);
        gateLineSecond->SetLineColor(gateCol);
        gateLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        gateLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(gateLineSecond);
    }

    TLine *gateLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    gateLine->SetLineColor(gateCol);
    gateLine->SetLineWidth(2);
    gateLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(gateLine);

    // When completing a pair, draw baseline and hatched region
    if (gate_markers.size() % 2 == 0) {
        const Int_t leftBin = static_cast<Int_t>(std::round(gate_markers[gate_markers.size() - 2]));
        const Int_t minB = std::min(leftBin, binX);
        const Int_t maxB = std::max(leftBin, binX);

        TLine *bottomGateLine = new TLine(minB - 0.5, 0.0, maxB - 0.5, 0.0);
        bottomGateLine->SetLineColor(gateCol);
        bottomGateLine->SetLineWidth(2);
        bottomGateLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(bottomGateLine);

        TBox *gateArea = new TBox(minB - 0.5, 0.0, maxB - 0.5, yMax);
        gateArea->SetFillColor(gateCol);
        gateArea->SetFillStyle(3354);
        gateArea->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gateArea);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::deleteGateMarkers
//==============================================================================
// Clears all stored coincidence gate markers (shortcut: 'Z + W').
//==============================================================================
void QMainCanvas::deleteGateMarkers()
{
    gate_markers.clear();
    const QString msg = "Coincidence gate markers cleared ('Z + W').\n";
    CommandPrompt::getInstance()->appendPlainText(msg);
    std::cout << msg.toStdString();
}

//==============================================================================
// QMainCanvas::showGateMarkers
//==============================================================================
// Re-renders all stored magenta coincidence gate markers and hatched intervals
// on the canvas (shortcut: 'M + W').
//==============================================================================
void QMainCanvas::showGateMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    Color_t gateCol = toMarkerColor(Design::getGateMarkerColor());
    for (std::size_t i = 0; i < gate_markers.size(); ++i) {
        TLine *gateLine = new TLine(gate_markers[i] - 0.5, 0.0,
                                    gate_markers[i] - 0.5, yMax);
        gateLine->SetLineColor(gateCol);
        gateLine->SetLineWidth(2);
        gateLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gateLine);

        if (i % 2 == 1) {
            const Int_t minB = static_cast<Int_t>(std::round(std::min(gate_markers[i - 1], gate_markers[i])));
            const Int_t maxB = static_cast<Int_t>(std::round(std::max(gate_markers[i - 1], gate_markers[i])));

            TLine *bottomGateLine = new TLine(minB - 0.5, 0.0, maxB - 0.5, 0.0);
            bottomGateLine->SetLineColor(gateCol);
            bottomGateLine->SetLineWidth(2);
            bottomGateLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomGateLine);

            TBox *gateArea = new TBox(minB - 0.5, 0.0, maxB - 0.5, yMax);
            gateArea->SetFillColor(gateCol);
            gateArea->SetFillStyle(3354);
            gateArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(gateArea);
        }
    }

    const QString msg = "Redrawing coincidence gate markers ('M + W').\n";
    CommandPrompt::getInstance()->appendPlainText(msg);
    std::cout << msg.toStdString();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::autoIntegrationAtCursor
//==============================================================================
// Triggered by 'A' then 'J'. Automatically finds the local peak maximum near
// the cursor, walks down to the valleys to set background markers, and 
// executes the area/integration calculation.
//==============================================================================
void QMainCanvas::autoIntegrationAtCursor(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    int searchWindow = 15; // Local max search window
    int maxBin = binX;
    double maxVal = hist->GetBinContent(binX);
    
    // Simple 3-point smoothing for finding the peak center
    auto getSmoothed = [&](int b) {
        if (b <= 1 || b >= hist->GetNbinsX()) return hist->GetBinContent(b);
        return (hist->GetBinContent(b-1) + 2*hist->GetBinContent(b) + hist->GetBinContent(b+1)) / 4.0;
    };

    // 1. Find true local maximum
    for (int b = std::max(1, binX - searchWindow); b <= std::min(hist->GetNbinsX(), binX + searchWindow); ++b) {
        double val = getSmoothed(b);
        if (val > maxVal) {
            maxVal = val;
            maxBin = b;
        }
    }

    // 2. Legacy-style walk down to valleys
    // We walk until the spectrum is concave-up (hit the tail) AND starts increasing,
    // using statistical error (sqrt) to prevent stopping on noise.
    int leftValley = maxBin - 1;
    while (leftValley > 1) {
        double vMinp = hist->GetBinContent(leftValley);
        double vMinpPrev = hist->GetBinContent(leftValley - 1);
        double vInpos = hist->GetBinContent(maxBin);
        int midBin = (leftValley + maxBin) / 2;
        double vMid = hist->GetBinContent(midBin);
        
        bool isConcaveUp = (vMinp + vInpos) > 2.0 * (vMid + 2.0 * std::sqrt(std::abs(vMid) + 1.0));
        bool isIncreasing = vMinp < vMinpPrev;
        
        if (isConcaveUp && isIncreasing) {
            break; 
        }
        leftValley--;
        if (maxBin - leftValley > 40) break; // sanity limit
    }

    int rightValley = maxBin + 1;
    while (rightValley < hist->GetNbinsX()) {
        double vMaxp = hist->GetBinContent(rightValley);
        double vMaxpNext = hist->GetBinContent(rightValley + 1);
        double vInpos = hist->GetBinContent(maxBin);
        int midBin = (rightValley + maxBin) / 2;
        double vMid = hist->GetBinContent(midBin);
        
        bool isConcaveUp = (vMaxp + vInpos) > 2.0 * (vMid + 2.0 * std::sqrt(std::abs(vMid) + 1.0));
        bool isIncreasing = vMaxp < vMaxpNext;
        
        if (isConcaveUp && isIncreasing) {
            break;
        }
        rightValley++;
        if (rightValley - maxBin > 40) break; // sanity limit
    }

    // Adjust in case of extremely narrow peak
    if (maxBin - leftValley < 3) leftValley = maxBin - 3;
    if (rightValley - maxBin < 3) rightValley = maxBin + 3;

    // Clear old markers
    background_markers.clear();
    integral_markers.clear();
    clearDrawnObjects();

    // Calculate background width based on peak width (min 2 channels)
    int peakWidth = rightValley - leftValley + 1;
    int bgWidth = std::max(2, peakWidth / 5);

    // Set background markers at valleys (two pairs: left region and right region)
    background_markers.push_back(static_cast<Double_t>(leftValley - bgWidth + 1));
    background_markers.push_back(static_cast<Double_t>(leftValley));
    background_markers.push_back(static_cast<Double_t>(rightValley));
    background_markers.push_back(static_cast<Double_t>(rightValley + bgWidth - 1));
    
    // Set integral markers inside the valleys
    integral_markers.push_back(static_cast<Double_t>(leftValley));
    integral_markers.push_back(static_cast<Double_t>(rightValley));

    // Draw the markers visually
    showBackgroundMarkers();
    showIntegralMarkers();

    // Execute integration with background (this will also open the dialog)
    areaFunctionWithBackground();
}

//==============================================================================
// QMainCanvas::showMJMarkers
//==============================================================================
void QMainCanvas::showMJMarkers()
{
    clearDrawnObjects();
    showBackgroundMarkers();
    showIntegralMarkers();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showMVMarkers
//==============================================================================
void QMainCanvas::showMVMarkers()
{
    clearDrawnObjects();
    showBackgroundMarkers();
    showRangeMarkers();
    showGaussMarkers();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::deleteZJMarkers
//==============================================================================
// Clears Background and Integral markers simultaneously (shortcut: 'Z + J').
//==============================================================================
void QMainCanvas::deleteZJMarkers()
{
    deleteBackgroundMarkers();
    deleteIntegralMarkers();
    clearDrawnObjects();
    if (canvas && canvas->getCanvas()) {
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    }
    CommandPrompt::getInstance()->appendPlainText("Background and Integral markers deleted (ZJ).\n");
}

//==============================================================================
// QMainCanvas::deleteZVMarkers
//==============================================================================
// Clears Background, Range, and Gauss markers simultaneously (shortcut: 'Z + V').
//==============================================================================
void QMainCanvas::deleteZVMarkers()
{
    deleteBackgroundMarkers();
    deleteRangeMarkers();
    deleteGaussMarkers();
    clearDrawnObjects();
    if (canvas && canvas->getCanvas()) {
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    }
    CommandPrompt::getInstance()->appendPlainText("Background, Range, and Gauss markers deleted (ZV).\n");
}

//==============================================================================
// QMainCanvas::drawZeroLine
//==============================================================================
// Draws a horizontal reference dashed line at zero counts across the visible
// spectrum window (shortcut: 'M + Z').
//==============================================================================
void QMainCanvas::drawZeroLine()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) return;

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    TVirtualPad *pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (!pad) pad = canvas->getCanvas();
    if (!pad) return;

    pad->cd();
    double xMin = hist->GetXaxis()->GetXmin();
    double xMax = hist->GetXaxis()->GetXmax();
    if (hist->GetXaxis()->GetFirst() > 1 || hist->GetXaxis()->GetLast() < hist->GetNbinsX()) {
        xMin = hist->GetXaxis()->GetBinLowEdge(hist->GetXaxis()->GetFirst());
        xMax = hist->GetXaxis()->GetBinUpEdge(hist->GetXaxis()->GetLast());
    }

    TLine *zeroLine = new TLine(xMin, 0.0, xMax, 0.0);
    zeroLine->SetLineColor(kGray + 2);
    zeroLine->SetLineStyle(2); // dashed
    zeroLine->SetLineWidth(2);
    zeroLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(zeroLine);

    pad->Modified();
    pad->Update();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    CommandPrompt::getInstance()->appendPlainText("Zero-level baseline line drawn (MZ).\n");
}

//==============================================================================
// QMainCanvas::deleteNearestGaussMarker
//==============================================================================
// Locates and deletes the Gaussian peak marker closest to the cursor position (shortcut: '-').
//==============================================================================
void QMainCanvas::deleteNearestGaussMarker(Int_t x, Int_t y)
{
    if (gauss_markers.empty()) {
        CommandPrompt::getInstance()->appendPlainText("No Gaussian peak centroid markers to delete (-).\n");
        return;
    }

    int binX = getBinFromClick(x, y);
    auto closestIt = gauss_markers.begin();
    int minDiff = std::abs(*closestIt - binX);

    for (auto it = gauss_markers.begin(); it != gauss_markers.end(); ++it) {
        int diff = std::abs(*it - binX);
        if (diff < minDiff) {
            minDiff = diff;
            closestIt = it;
        }
    }

    int deletedBin = *closestIt;
    gauss_markers.erase(closestIt);

    clearDrawnObjects();
    showGaussMarkers();
    if (canvas && canvas->getCanvas()) {
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    }

    CommandPrompt::getInstance()->appendPlainText(QString("Deleted nearest Gauss marker at channel %1 (-).\n").arg(deletedBin));
}

#include <QInputDialog>
#include <QMessageBox>

//==============================================================================
// QMainCanvas::quickEnergyCalibration
//==============================================================================
void QMainCanvas::quickEnergyCalibration()
{
    if (range_markers.size() < 2) {
        CommandPrompt::getInstance()->appendPlainText("Error: Quick calibration requires 2 range markers (use 'R').\n");
        return;
    }

    // Sort markers to ensure left < right
    std::vector<double> sortedMarkers = range_markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end());
    
    // We only use the first two range markers.
    int bin1 = sortedMarkers[0];
    int bin2 = sortedMarkers[1];

    TracknHistogram *hist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!hist) return;

    // Snap to the local maximum within +/- 15 bins to ensure we hit the true peak centroid
    int searchWindow = 15;
    
    auto findLocalMax = [&](int startBin) -> int {
        int bestBin = startBin;
        double maxVal = -1e9;
        int minSearch = std::max(1, startBin - searchWindow);
        int maxSearch = std::min(hist->GetNbinsX(), startBin + searchWindow);
        for (int b = minSearch; b <= maxSearch; ++b) {
            double val = hist->GetBinContent(b);
            if (val > maxVal) {
                maxVal = val;
                bestBin = b;
            }
        }
        return bestBin;
    };

    bin1 = findLocalMax(bin1);
    bin2 = findLocalMax(bin2);

    if (bin1 == bin2) {
        CommandPrompt::getInstance()->appendPlainText("Error: Markers snapped to the same peak.\n");
        return;
    }

    // Convert bin to 0-indexed channel (ROOT bin N corresponds to X-axis [N-1, N], center N-0.5. True channel = N - 1)
    double ch1 = bin1 - 1.0;
    double ch2 = bin2 - 1.0;

    bool ok1, ok2;
    double e1 = QInputDialog::getDouble(this, "Quick Calibration",
                                        QString("Energy for peak at channel %1:").arg(ch1),
                                        1173.238, 0, 100000, 3, &ok1);
    if (!ok1) return;

    double e2 = QInputDialog::getDouble(this, "Quick Calibration",
                                        QString("Energy for peak at channel %1:").arg(ch2),
                                        1332.513, 0, 100000, 3, &ok2);
    if (!ok2) return;

    if (e1 == e2) {
        CommandPrompt::getInstance()->appendPlainText("Error: Energies must be different.\n");
        return;
    }

    double slope = (e2 - e1) / (ch2 - ch1);
    double intercept = e1 - slope * ch1;

    if (hist) {
        hist->SetCalibration(intercept, slope, 0.0);
        CommandPrompt::getInstance()->appendPlainText(
            QString("Calibration applied: A(0)=%1, A(1)=%2\n").arg(intercept).arg(slope)
        );
        clearTheScreen();
    }
}

//==============================================================================
// QMainCanvas::showMatrixProjection
//==============================================================================
void QMainCanvas::showMatrixProjection()
{
    if (!m_currentMatrix || !m_currentMatrix->isOpen()) {
        CommandPrompt::getInstance()->appendPlainText("Error: No compressed matrix loaded (use Open CM).\n");
        return;
    }

    const std::vector<double> &proj = m_currentMatrix->getProjection();
    QString title = m_currentMatrix->getFileName() + " Projection";

    loadSpectrumDataToPad(proj, title, false);
    CommandPrompt::getInstance()->appendPlainText("Loaded full matrix projection.\n");
}
