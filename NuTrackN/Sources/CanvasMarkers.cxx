#include "canvas.h"
#include "Design.h"
#include "Integral.h"
#include "PeakFit.h"
#include "tracknhistogram.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TLine.h>
#include <TBox.h>
#include <TLatex.h>
#include <TVirtualPad.h>
#include <QTimer>

#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

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

    // Create a cyan vertical marker line and draw it over the spectrum
    TLine *spacebarLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    spacebarLine->SetLineColor(kCyan);
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
        lbl->SetTextFont(43);
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
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
void QMainCanvas::areaFunctionWithBackground()
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

    // If background markers exist, draw blue baseline
    if (!background_markers.empty() && background_markers.size() >= 2) {
        const Double_t xStart = background_markers.front() - 0.5;
        const Double_t xEnd   = background_markers.back() - 0.5;
        TLine *backgroundLine = new TLine(xStart, slope * xStart + addition,
                                          xEnd,   slope * xEnd   + addition);
        backgroundLine->SetLineColor(kBlue);
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
        lbl->SetTextFont(43);
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
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

    // When placing the second marker of a pair, ensure previous boundary is redrawn
    if (background_markers.size() % 2 == 0 && !background_markers.empty()) {
        const std::size_t i = background_markers.size();
        TLine *backgroundLineSecond = new TLine(background_markers[i - 2] - 0.5, 0.0,
                                                background_markers[i - 2] - 0.5, yMax);
        backgroundLineSecond->SetLineColor(kBlue);
        backgroundLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        backgroundLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(backgroundLineSecond);
    }

    // Draw vertical boundary line at current clicked position
    TLine *backgroundLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);
    backgroundLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(backgroundLine);

    // When completing a pair, draw baseline and hatched region
    if (background_markers.size() % 2 == 0) {
        const Int_t leftBin = background_markers[background_markers.size() - 2];
        TLine *bottomBackgroundLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
        bottomBackgroundLine->SetLineColor(kBlue);
        bottomBackgroundLine->SetLineWidth(2);
        bottomBackgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

        TBox *backgroundArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
        backgroundArea->SetFillColor(kBlue);
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

    // If second marker of pair, ensure the first is properly drawn
    if (integral_markers.size() % 2 == 0 && !integral_markers.empty()) {
        const std::size_t i = integral_markers.size();
        TLine *integralLineSecond = new TLine(integral_markers[i - 2] - 0.5, 0.0,
                                              integral_markers[i - 2] - 0.5, yMax);
        integralLineSecond->SetLineColor(kYellow);
        integralLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        integralLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(integralLineSecond);
    }

    TLine *integralLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    integralLine->SetLineColor(kYellow);
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

    for (std::size_t i = 0; i < background_markers.size(); ++i) {
        TLine *backgroundLine = new TLine(background_markers[i] - 0.5, 0.0,
                                          background_markers[i] - 0.5, yMax);
        backgroundLine->SetLineColor(kBlue);
        backgroundLine->SetLineWidth(2);
        backgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundLine);

        // When completing a pair, redraw baseline and hatched box
        if (i % 2 == 1) {
            TLine *bottomBackgroundLine = new TLine(background_markers[i - 1] - 0.5, 0.0,
                                                    background_markers[i] - 0.5, 0.0);
            bottomBackgroundLine->SetLineColor(kBlue);
            bottomBackgroundLine->SetLineWidth(2);
            bottomBackgroundLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

            TBox *backgroundArea = new TBox(background_markers[i - 1] - 0.5, 0.0,
                                            background_markers[i] - 0.5, yMax);
            backgroundArea->SetFillColor(kBlue);
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

    for (std::size_t i = 0; i < integral_markers.size(); ++i) {
        TLine *integralLine = new TLine(integral_markers[i] - 0.5, 0.0,
                                        integral_markers[i] - 0.5, yMax);
        integralLine->SetLineColor(kYellow);
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

        // When placing the second marker of a pair, ensure the first is drawn
        if (range_markers.size() % 2 == 0 && !range_markers.empty()) {
            const std::size_t i = range_markers.size();
            TLine *rangeLineSecond = new TLine(range_markers[i - 2] - 0.5, 0.0,
                                               range_markers[i - 2] - 0.5, yMax);
            rangeLineSecond->SetLineColor(kYellow);
            rangeLineSecond->SetLineWidth(2);
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            rangeLineSecond->Draw();
            listOfObjectsDrawnOnScreen.Add(rangeLineSecond);
        }

        TLine *rangeLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
        rangeLine->SetLineColor(kYellow);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        // If pair is complete, draw baseline and shaded yellow region
        if (range_markers.size() % 2 == 0) {
            const Int_t leftBin = range_markers[range_markers.size() - 2];
            TLine *bottomRangeLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
            bottomRangeLine->SetLineColor(kYellow);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
            rangeArea->SetFillColor(kYellow);
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

    for (std::size_t i = 0; i < range_markers.size(); ++i) {
        TLine *rangeLine = new TLine(range_markers[i] - 0.5, 0.0,
                                     range_markers[i] - 0.5, yMax);
        rangeLine->SetLineColor(kYellow);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        if (i % 2 == 1) {
            TLine *bottomRangeLine = new TLine(range_markers[i - 1] - 0.5, 0.0,
                                               range_markers[i] - 0.5, 0.0);
            bottomRangeLine->SetLineColor(kYellow);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(range_markers[i - 1] - 0.5, 0.0,
                                       range_markers[i] - 0.5, yMax);
            rangeArea->SetFillColor(kYellow);
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
    gaussLine->SetLineColor(kPink);
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
// Re-renders all stored pink Gaussian centroid estimate markers on the canvas
// (shortcut: 'M + G').
//==============================================================================
void QMainCanvas::showGaussMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    for (std::size_t i = 0; i < gauss_markers.size(); ++i) {
        TLine *gaussLine = new TLine(gauss_markers[i] - 0.5, 0.0,
                                     gauss_markers[i] - 0.5, yMax);
        gaussLine->SetLineColor(kPink);
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

    // When placing the second marker of a pair, ensure previous boundary is redrawn
    if (gate_markers.size() % 2 == 0 && !gate_markers.empty()) {
        const std::size_t i = gate_markers.size();
        TLine *gateLineSecond = new TLine(gate_markers[i - 2] - 0.5, 0.0,
                                          gate_markers[i - 2] - 0.5, yMax);
        gateLineSecond->SetLineColor(kMagenta);
        gateLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        gateLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(gateLineSecond);
    }

    TLine *gateLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    gateLine->SetLineColor(kMagenta);
    gateLine->SetLineWidth(2);
    gateLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(gateLine);

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    bool isCalib = trackHist ? trackHist->IsCalibrated() : false;

    // When completing a pair, draw baseline and hatched region
    if (gate_markers.size() % 2 == 0) {
        const Int_t leftBin = static_cast<Int_t>(std::round(gate_markers[gate_markers.size() - 2]));
        const Int_t minB = std::min(leftBin, binX);
        const Int_t maxB = std::max(leftBin, binX);

        TLine *bottomGateLine = new TLine(minB - 0.5, 0.0, maxB - 0.5, 0.0);
        bottomGateLine->SetLineColor(kMagenta);
        bottomGateLine->SetLineWidth(2);
        bottomGateLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(bottomGateLine);

        TBox *gateArea = new TBox(minB - 0.5, 0.0, maxB - 0.5, yMax);
        gateArea->SetFillColor(kMagenta);
        gateArea->SetFillStyle(3354);
        gateArea->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gateArea);

        double eMin = isCalib ? (trackHist->GetCalibA0() + trackHist->GetCalibA1() * minB + trackHist->GetCalibA2() * minB * minB) : minB;
        double eMax = isCalib ? (trackHist->GetCalibA0() + trackHist->GetCalibA1() * maxB + trackHist->GetCalibA2() * maxB * maxB) : maxB;
        QString msg = QString("Gate #%1 defined: [%2, %3]").arg(gate_markers.size() / 2).arg(minB).arg(maxB);
        if (isCalib) {
            msg += QString(" (%.1f - %.1f keV)").arg(eMin, 0, 'f', 1).arg(eMax, 0, 'f', 1);
        }
        msg += QString(", width = %1 ch. Press 'C + W' or click 'Gate CM' to slice.\n").arg(maxB - minB + 1);
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
    } else {
        double eX = isCalib ? (trackHist->GetCalibA0() + trackHist->GetCalibA1() * binX + trackHist->GetCalibA2() * binX * binX) : binX;
        QString msg = QString("Gate marker #%1 placed at ch %2").arg((gate_markers.size() + 1) / 2).arg(binX);
        if (isCalib) {
            msg += QString(" (%.1f keV)").arg(eX, 0, 'f', 1);
        }
        msg += ". Place second marker with 'W' to define gate.\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
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

    for (std::size_t i = 0; i < gate_markers.size(); ++i) {
        TLine *gateLine = new TLine(gate_markers[i] - 0.5, 0.0,
                                    gate_markers[i] - 0.5, yMax);
        gateLine->SetLineColor(kMagenta);
        gateLine->SetLineWidth(2);
        gateLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gateLine);

        if (i % 2 == 1) {
            const Int_t minB = static_cast<Int_t>(std::round(std::min(gate_markers[i - 1], gate_markers[i])));
            const Int_t maxB = static_cast<Int_t>(std::round(std::max(gate_markers[i - 1], gate_markers[i])));

            TLine *bottomGateLine = new TLine(minB - 0.5, 0.0, maxB - 0.5, 0.0);
            bottomGateLine->SetLineColor(kMagenta);
            bottomGateLine->SetLineWidth(2);
            bottomGateLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomGateLine);

            TBox *gateArea = new TBox(minB - 0.5, 0.0, maxB - 0.5, yMax);
            gateArea->SetFillColor(kMagenta);
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
