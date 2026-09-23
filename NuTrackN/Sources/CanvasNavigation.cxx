#include "canvas.h"
#include "Design.h"
#include "tracknhistogram.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TVirtualPad.h>
#include <TAxis.h>
#include <TLatex.h>
#include <TList.h>

#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

//==============================================================================
// QMainCanvas::zoomTheScreen
//==============================================================================
// Executes zoom between the two most recently placed spacebar markers.
// Triggered by the 'E' key shortcut.
//==============================================================================
void QMainCanvas::zoomTheScreen()
{
    // Clear temporary overlay markers before applying zoom
    clearDrawnObjects();

    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    const std::size_t n = zoom_markers.size();

    if (n >= 2) {
        const double low  = std::min(zoom_markers[n - 2], zoom_markers[n - 1]);
        const double high = std::max(zoom_markers[n - 2], zoom_markers[n - 1]);
        if (HijF[SelectedElement_i][SelectedElement_j]) {
            TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
            hist->GetXaxis()->SetRangeUser(low, high);
            adjustYAxisToVisibleMax(hist);
        }
    } else {
        const QString msg = "At least two spacebar markers are required to define a zoom window.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::fullX
//==============================================================================
// Unzooms the horizontal X-axis to the full spectrum range (channels 0 to max),
// while preserving the current vertical Y-axis scale/zoom (matching GASP 'FX').
// Triggered by 'F + X' keyboard shortcut or 'FX' toolbar button.
//==============================================================================
void QMainCanvas::fullX(bool logPrompt)
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        hist->GetXaxis()->UnZoom();
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
    if (logPrompt) {
        CommandPrompt::getInstance()->appendPlainText("Horizontal axis unzoomed to full spectrum (FX).\n");
    }
}

//==============================================================================
// QMainCanvas::fullY
//==============================================================================
// Autoscales the vertical Y-axis to the highest peak visible within the current
// horizontal zoom window with 10% headroom (matching GASP 'FY').
// Triggered by 'F + Y' keyboard shortcut or 'FY' toolbar button.
//==============================================================================
void QMainCanvas::fullY(bool logPrompt)
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
    if (logPrompt) {
        CommandPrompt::getInstance()->appendPlainText("Vertical axis autoscaled to visible maximum (FY).\n");
    }
}

//==============================================================================
// QMainCanvas::zoomOut
//==============================================================================
// Full display (FF): resets spectrum to full horizontal range and autoscales
// vertical scale to the global maximum. Implemented as FX + FY.
// Triggered by 'F + F' / 'F + S' keyboard shortcuts or 'FF' toolbar button.
//==============================================================================
void QMainCanvas::zoomOut()
{
    fullX(false);
    fullY(false);
    CommandPrompt::getInstance()->appendPlainText("Full spectrum display restored (FF = FX + FY).\n");
}

//==============================================================================
// QMainCanvas::sameX
//==============================================================================
// Propagates the horizontal X-axis zoom range (min/max channels or energy)
// of the active histogram to all displayed histograms/pads on the screen.
// Triggered by 'S + X' keyboard shortcut or the 'SX' toolbar button (matching GASP 'SX').
//==============================================================================
void QMainCanvas::sameX()
{
    if (SelectedElement_i < 1 || SelectedElement_i > maxElement_i ||
        SelectedElement_j < 1 || SelectedElement_j > maxElement_j) return;
    TH1F *activeHist = HijF[SelectedElement_i][SelectedElement_j];
    if (!activeHist || !canvas || !canvas->getCanvas()) return;

    TAxis *activeXAxis = activeHist->GetXaxis();
    if (!activeXAxis) return;

    const Int_t firstBin = activeXAxis->GetFirst();
    const Int_t lastBin  = activeXAxis->GetLast();
    const Double_t xMin  = activeXAxis->GetBinLowEdge(firstBin);
    const Double_t xMax  = activeXAxis->GetBinUpEdge(lastBin);
    const bool isUnzoomed = (firstBin <= 1 && lastBin >= activeHist->GetNbinsX());

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TH1F *targetHist = HijF[z][g];
            if (!targetHist) continue;

            if (isUnzoomed) {
                targetHist->GetXaxis()->UnZoom();
            } else {
                targetHist->GetXaxis()->SetRangeUser(xMin, xMax);
            }

            renderPeakSearchLabels(z, g);
            renderPeakLabels(z, g);

            TVirtualPad *pad = nullptr;
            if (maxElement_i > 1 || maxElement_j > 1) {
                pad = canvas->getCanvas()->GetPad((z - 1) * maxElement_j + g);
            } else {
                pad = canvas->getCanvas();
            }
            if (pad) {
                pad->Modified();
                pad->Update();
            }
        }
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
    CommandPrompt::getInstance()->appendPlainText("Applied same X zoom to all display windows (SX).\n");
}

//==============================================================================
// QMainCanvas::sameY
//==============================================================================
// Propagates the vertical Y-axis scale (minimum, maximum, and linear/log mode)
// of the active histogram to all displayed histograms/pads on the screen.
// Triggered by 'S + Y' keyboard shortcut or the 'SY' toolbar button (matching GASP 'SY').
//==============================================================================
void QMainCanvas::sameY()
{
    if (SelectedElement_i < 1 || SelectedElement_i > maxElement_i ||
        SelectedElement_j < 1 || SelectedElement_j > maxElement_j) return;
    TH1F *activeHist = HijF[SelectedElement_i][SelectedElement_j];
    if (!activeHist || !canvas || !canvas->getCanvas()) return;

    TVirtualPad *activePad = nullptr;
    if (maxElement_i > 1 || maxElement_j > 1) {
        activePad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    } else {
        activePad = canvas->getCanvas();
    }
    const int activeLogy = activePad ? activePad->GetLogy() : 0;
    double activeMin = activeHist->GetMinimum();
    double activeMax = activeHist->GetMaximum();
    if (activeMin < 0.0 || activeMin == -1111.0) {
        activeMin = (activeLogy != 0) ? 0.5 : 0.0;
    }
    if (activeMax <= activeMin || activeMax == -1111.0) {
        activeMax = activeHist->GetBinContent(activeHist->GetMaximumBin()) * ((activeLogy != 0) ? 1.30 : 1.10);
        if (activeMax <= activeMin) activeMax = activeMin + 10.0;
    }
    if (activeLogy != 0 && activeMin <= 0.0) {
        activeMin = 0.5;
    }

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TH1F *targetHist = HijF[z][g];
            if (!targetHist) continue;

            TVirtualPad *pad = nullptr;
            if (maxElement_i > 1 || maxElement_j > 1) {
                pad = canvas->getCanvas()->GetPad((z - 1) * maxElement_j + g);
            } else {
                pad = canvas->getCanvas();
            }
            if (pad) {
                pad->SetLogy(activeLogy);
            }

            targetHist->SetMinimum(activeMin);
            targetHist->SetMaximum(activeMax);
            targetHist->GetYaxis()->SetRangeUser(activeMin, activeMax);

            for (TH1F *overlay : HijC[z][g]) {
                if (!overlay || overlay == targetHist) continue;
                overlay->SetMinimum(activeMin);
                overlay->SetMaximum(activeMax);
            }

            renderPeakSearchLabels(z, g);
            renderPeakLabels(z, g);

            if (pad) {
                pad->Modified();
                pad->Update();
            }
        }
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
    CommandPrompt::getInstance()->appendPlainText("Applied same Y scale and mode to all display windows (SY).\n");
}

//==============================================================================
// QMainCanvas::toggleLogY
//==============================================================================
// Toggles logarithmic vs linear Y-axis scale on the active histogram pad
// (corresponding to the 'L' button in Xtrackn).
//==============================================================================
void QMainCanvas::toggleLogY()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) return;
    if (!canvas || !canvas->getCanvas()) return;

    TVirtualPad *pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (!pad) pad = canvas->getCanvas();
    if (!pad) return;

    const int newLogy = pad->GetLogy() ? 0 : 1;
    pad->SetLogy(newLogy);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        Int_t firstBin = xAxis ? xAxis->GetFirst() : 1;
        Int_t lastBin  = xAxis ? xAxis->GetLast() : hist->GetNbinsX();
        if (firstBin < 1) firstBin = 1;
        if (lastBin > hist->GetNbinsX()) lastBin = hist->GetNbinsX();

        double localMax = 0.0;
        for (Int_t b = firstBin; b <= lastBin; ++b) {
            double content = hist->GetBinContent(b);
            if (content > localMax) {
                localMax = content;
            }
        }
        for (TH1F *overlay : HijC[SelectedElement_i][SelectedElement_j]) {
            if (!overlay || overlay == hist) continue;
            Int_t ovFirst = firstBin;
            Int_t ovLast  = lastBin;
            if (ovFirst < 1) ovFirst = 1;
            if (ovLast > overlay->GetNbinsX()) ovLast = overlay->GetNbinsX();
            for (Int_t b = ovFirst; b <= ovLast; ++b) {
                double content = overlay->GetBinContent(b);
                if (content > localMax) {
                    localMax = content;
                }
            }
        }
        if (localMax <= 0.0) localMax = 10.0;

        if (newLogy) {
            hist->SetMinimum(0.5);
            hist->GetYaxis()->SetRangeUser(0.5, localMax * 1.30);
            hist->SetMaximum(localMax * 1.30);
        } else {
            hist->SetMinimum(0.0);
            hist->GetYaxis()->SetRangeUser(0.0, localMax * 1.10);
            hist->SetMaximum(localMax * 1.10);
        }
    }

    // Apply matching minimum to all overlays on this pad
    for (TH1F *overlay : HijC[SelectedElement_i][SelectedElement_j]) {
        if (!overlay || overlay == hist) continue;
        if (newLogy) {
            overlay->SetMinimum(0.5);
        } else {
            overlay->SetMinimum(0.0);
        }
    }

    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    ColorTheFrameOfTheHistogram();
    pad->Modified();
    pad->Update();

    CommandPrompt::getInstance()->appendPlainText(newLogy ? "Scale set to Logarithmic (L).\n"
                                                           : "Scale set to Linear (L).\n");

    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::adjustYAxisToVisibleMax
//==============================================================================
// Scans the visible X-axis bin range of the given histogram, calculates the
// highest count within that range, and sets the Y-axis maximum to leave empty
// space at the top of the spectrum so the largest peak does not touch the
// top border (10% headroom in linear scale, 30% in log scale).
//==============================================================================
void QMainCanvas::adjustYAxisToVisibleMax(TH1F *hist, int z, int g)
{
    if (!hist) return;
    TAxis *xAxis = hist->GetXaxis();
    if (!xAxis) return;

    Int_t firstBin = xAxis->GetFirst();
    Int_t lastBin  = xAxis->GetLast();
    if (firstBin < 1) firstBin = 1;
    if (lastBin > hist->GetNbinsX()) lastBin = hist->GetNbinsX();

    double localMax = 0.0;
    for (Int_t b = firstBin; b <= lastBin; ++b) {
        double content = hist->GetBinContent(b);
        if (content > localMax) {
            localMax = content;
        }
    }

    // Also consider overlaid spectra on the target pad to avoid clipping peaks
    const int targetZ = (z >= 1 && z < 12) ? z : SelectedElement_i;
    const int targetG = (g >= 1 && g < 12) ? g : SelectedElement_j;
    if (targetZ >= 1 && targetZ < 12 && targetG >= 1 && targetG < 12) {
        for (TH1F *overlay : HijC[targetZ][targetG]) {
            if (!overlay || overlay == hist) continue;
            Int_t ovFirst = firstBin;
            Int_t ovLast  = lastBin;
            if (ovFirst < 1) ovFirst = 1;
            if (ovLast > overlay->GetNbinsX()) ovLast = overlay->GetNbinsX();
            for (Int_t b = ovFirst; b <= ovLast; ++b) {
                double content = overlay->GetBinContent(b);
                if (content > localMax) {
                    localMax = content;
                }
            }
        }
    }

    if (localMax <= 0.0) {
        localMax = 10.0;
    }

    TVirtualPad *pad = nullptr;
    if (canvas && canvas->getCanvas()) {
        pad = canvas->getCanvas()->GetPad((targetZ - 1) * maxElement_j + targetG);
        if (!pad) pad = canvas->getCanvas();
    }
    const bool isLog = (pad && pad->GetLogy() != 0);
    const double yMin = isLog ? 0.5 : 0.0;
    const double yMax = isLog ? (localMax <= 0.0 ? 10.0 : localMax * 1.30)
                              : (localMax <= 0.0 ? 10.0 : localMax * 1.10);

    hist->GetYaxis()->SetRangeUser(yMin, yMax);
    hist->SetMaximum(yMax);
    hist->SetMinimum(yMin);

    if (targetZ >= 1 && targetZ < 12 && targetG >= 1 && targetG < 12) {
        for (TH1F *overlay : HijC[targetZ][targetG]) {
            if (!overlay || overlay == hist) continue;
            overlay->SetMinimum(yMin);
        }
    }
}

//==============================================================================
// QMainCanvas::renderPeakLabels
//==============================================================================
// Removes any existing TLatex peak centroid annotations from the specified pad
// and renders the current peak list (gaussCenters) with a fixed pixel font size
// (does not resize with the window), displaying calibrated energy if available.
//==============================================================================
void QMainCanvas::renderPeakLabels(int z, int g)
{
    if (z < 1 || z >= 12 || g < 1 || g >= 12) return;
    if (!HijF[z][g] || !canvas || !canvas->getCanvas()) return;

    if (maxElement_i > 1 || maxElement_j > 1) {
        int padIndex = (z - 1) * maxElement_j + g;
        canvas->getCanvas()->cd(padIndex);
    } else {
        canvas->getCanvas()->cd();
    }
    TVirtualPad *pad = gPad;
    if (!pad) return;

    // Remove any previously drawn peak labels from the pad to avoid overlapping text
    TList *primitives = pad->GetListOfPrimitives();
    if (primitives) {
        std::vector<TObject*> toRemove;
        TIter next(primitives);
        while (TObject *obj = next()) {
            if (obj && obj->InheritsFrom(TLatex::Class())) {
                TNamed *named = dynamic_cast<TNamed*>(obj);
                if (named && strcmp(named->GetName(), "PeakLabel") == 0) {
                    toRemove.push_back(obj);
                }
            }
        }
        for (TObject *obj : toRemove) {
            primitives->Remove(obj);
            delete obj;
        }
    }

    // Render each fitted peak label with fixed pixel font size
    TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(HijF[z][g]);
    for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
        Double_t center = gaussCenters[z][g][k];
        Double_t height = (k < gaussCentersHeight[z][g].size()) ? gaussCentersHeight[z][g][k] : 0.0;
        char buffer[64];
        if (tHist && tHist->IsCalibrated()) {
            snprintf(buffer, sizeof(buffer), "%.2f", tHist->ChannelToEnergy(center));
        } else {
            snprintf(buffer, sizeof(buffer), "%.2f", center);
        }

        TLatex *lbl = new TLatex(center, height, buffer);
        lbl->SetName("PeakLabel");
        lbl->SetTextFont(43); // Font 4 (Helvetica), Precision 3 (exact screen pixels)
        lbl->SetTextSize(20); // Fixed 20-pixel font size (increased by ~50% from 13px)
        lbl->SetTextAlign(21); // Centered horizontally at peak center, bottom-aligned
        lbl->Draw();
    }

    pad->Modified();
    pad->Update();
}

//==============================================================================
// QMainCanvas::updateAxisStatusLabels
//==============================================================================
// Updates the X Min, X Max, Y Min, and Y Max status display labels in the
// top status header bar to reflect the current visible axis ranges.
//==============================================================================
void QMainCanvas::updateAxisStatusLabels()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    TAxis *xAxis = hist->GetXaxis();
    double xMin = xAxis ? xAxis->GetBinLowEdge(xAxis->GetFirst()) : 0.0;
    double xMax = xAxis ? xAxis->GetBinUpEdge(xAxis->GetLast()) : 0.0;
    double yMin = hist->GetMinimum();
    double yMax = hist->GetMaximum();

    TVirtualPad *pad = nullptr;
    if (canvas && canvas->getCanvas()) {
        pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
    }
    const bool isLog = (pad && pad->GetLogy() != 0);
    if (yMin < 0.0 || yMin == -1111.0) {
        yMin = isLog ? 0.5 : 0.0;
    }
    if (yMax <= yMin || yMax == -1111.0) {
        yMax = hist->GetBinContent(hist->GetMaximumBin()) * (isLog ? 1.30 : 1.10);
        if (yMax <= yMin) yMax = yMin + 10.0;
    }

    auto formatCoord = [](double val) -> QString {
        if (std::abs(val) <= 100000.0) {
            if (std::abs(val - std::round(val)) < 1e-5) {
                return QString::number(static_cast<long long>(std::round(val)));
            } else {
                return QString::number(val, 'f', 2);
            }
        } else {
            return QString::number(val, 'e', 2);
        }
    };

    if (labelXMin) labelXMin->setText(QString("X Min: %1").arg(formatCoord(xMin)));
    if (labelXMax) labelXMax->setText(QString("X Max: %1").arg(formatCoord(xMax)));
    if (labelYMin) labelYMin->setText(QString("Y Min: %1").arg(formatCoord(yMin)));
    if (labelYMax) labelYMax->setText(QString("Y Max: %1").arg(formatCoord(yMax)));
}

//==============================================================================
// QMainCanvas::translateplusTheScreen
//==============================================================================
// Pans the spectrum display horizontally to the right (toward higher channels)
// by ~3% of the visible window width. Triggered by the Right Arrow key.
//==============================================================================
void QMainCanvas::translateplusTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Remove drawn lines/markers when panning
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first  = xAxis->GetFirst();
        const Int_t last   = xAxis->GetLast();
        const Int_t step   = std::max(1, (last - first) / 33);
        const Int_t maxBin = hist->GetNbinsX();
        const Int_t newFirst = std::min(maxBin, first + step);
        const Int_t newLast  = std::min(maxBin, last + step);
        xAxis->SetRange(newFirst, newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::translateminusTheScreen
//==============================================================================
// Pans the spectrum display horizontally to the left (toward lower channels)
// by ~3% of the visible window width. Triggered by the Left Arrow key.
//==============================================================================
void QMainCanvas::translateminusTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Remove drawn lines/markers when panning
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first    = xAxis->GetFirst();
        const Int_t last     = xAxis->GetLast();
        const Int_t step     = std::max(1, (last - first) / 33);
        const Int_t newFirst = std::max(1, first - step);
        const Int_t newLast  = std::max(1, last - step);
        xAxis->SetRange(newFirst, newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::shiftDisplayLeft75
//==============================================================================
// Pans the spectrum display horizontally to the left (toward lower channels)
// by 75% (3/4) of the visible window width. Triggered by '<' or ','.
//==============================================================================
void QMainCanvas::shiftDisplayLeft75()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first    = xAxis->GetFirst();
        const Int_t last     = xAxis->GetLast();
        const Int_t width    = last - first;
        const Int_t step     = std::max(1, static_cast<Int_t>(width * 0.75));
        const Int_t newFirst = std::max(1, first - step);
        const Int_t newLast  = std::max(newFirst + 2, last - step);
        xAxis->SetRange(newFirst, newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    CommandPrompt::getInstance()->appendPlainText("Shifted display 3/4 left (<).\n");
}

//==============================================================================
// QMainCanvas::shiftDisplayRight75
//==============================================================================
// Pans the spectrum display horizontally to the right (toward higher channels)
// by 75% (3/4) of the visible window width. Triggered by '>' or '.'.
//==============================================================================
void QMainCanvas::shiftDisplayRight75()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first    = xAxis->GetFirst();
        const Int_t last     = xAxis->GetLast();
        const Int_t width    = last - first;
        const Int_t step     = std::max(1, static_cast<Int_t>(width * 0.75));
        const Int_t maxBin   = hist->GetNbinsX();
        const Int_t newLast  = std::min(maxBin, last + step);
        const Int_t newFirst = std::min(newLast - 2, first + step);
        xAxis->SetRange(std::max(1, newFirst), newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    CommandPrompt::getInstance()->appendPlainText("Shifted display 3/4 right (>).\n");
}

//==============================================================================
// QMainCanvas::translatedownTheScreen
//==============================================================================
// Expands the vertical count scale (zooms out vertically by 1.10x).
// Triggered by the Down Arrow key or mouse wheel scroll up.
//==============================================================================
void QMainCanvas::translatedownTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TVirtualPad *pad = nullptr;
    if (canvas && canvas->getCanvas()) {
        pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
    }
    const bool isLog = (pad && pad->GetLogy() != 0);
    const double curMin = isLog ? 0.5 : 0.0;

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        double curMax = hist->GetMaximum();
        if (curMax <= curMin) curMax = 10.0;
        const double newMax = isLog ? curMax * 1.25 : curMax * 1.10;
        hist->GetYaxis()->SetRangeUser(curMin, newMax);
        hist->SetMaximum(newMax);
        hist->SetMinimum(curMin);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::translateupTheScreen
//==============================================================================
// Compresses the vertical count scale (zooms in vertically by 1.10x).
// Triggered by the Up Arrow key or mouse wheel scroll down.
//==============================================================================
void QMainCanvas::translateupTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TVirtualPad *pad = nullptr;
    if (canvas && canvas->getCanvas()) {
        pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
    }
    const bool isLog = (pad && pad->GetLogy() != 0);
    const double curMin = isLog ? 0.5 : 0.0;

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        double curMax = hist->GetMaximum();
        if (curMax > (isLog ? curMin + 2.0 : 2.0)) {
            const double newMax = isLog ? curMax / 1.25 : curMax / 1.10;
            if (newMax > curMin + 1.0) {
                hist->GetYaxis()->SetRangeUser(curMin, newMax);
                hist->SetMaximum(newMax);
                hist->SetMinimum(curMin);
            }
        }
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::adjustAxisRange
//==============================================================================
// Implements Xtrackn Table 2 interactive mouse clicking on axis range labels:
// - Left Click: increases / shifts range limit positively
// - Right Click: decreases / shifts range limit negatively
// - No modifier: Coarse step of 20.0% of the active span
// - Ctrl modifier: Fine step of 2.5% of the active span
//==============================================================================
void QMainCanvas::adjustAxisRange(const QString &axisName, bool increase, bool fineStep)
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    clearDrawnObjects();

    TAxis *xAxis = hist->GetXaxis();
    if (!xAxis) return;

    double xMin = xAxis->GetBinLowEdge(xAxis->GetFirst());
    double xMax = xAxis->GetBinUpEdge(xAxis->GetLast());
    double yMin = hist->GetMinimum();
    double yMax = hist->GetMaximum();

    TVirtualPad *pad = nullptr;
    if (canvas && canvas->getCanvas()) {
        pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
    }
    const bool isLog = (pad && pad->GetLogy() != 0);
    const double logFloor = 0.5;
    if (yMin < 0.0 || yMin == -1111.0) {
        yMin = isLog ? logFloor : 0.0;
    }
    if (yMax <= yMin || yMax == -1111.0) {
        yMax = hist->GetBinContent(hist->GetMaximumBin()) * (isLog ? 1.30 : 1.10);
        if (yMax <= yMin) yMax = yMin + 10.0;
    }

    const double shiftFactor = fineStep ? 0.025 : 0.200;
    const double xSpan = std::max(1.0, xMax - xMin);
    const double ySpan = std::max(1.0, yMax - yMin);
    const double maxChannel = hist->GetNbinsX();

    if (axisName == "XMin") {
        if (increase) {
            xMin += std::round(shiftFactor * xSpan) + 1.0;
            if (xMin >= xMax) xMin = xMax - 1.0;
        } else {
            xMin -= std::round(shiftFactor * xSpan) + 1.0;
            if (xMin < 0.0) xMin = 0.0;
        }
        xAxis->SetRangeUser(xMin, xMax);
    } else if (axisName == "XMax") {
        if (increase) {
            xMax += std::round(shiftFactor * xSpan) + 1.0;
            if (xMax > maxChannel) xMax = maxChannel;
        } else {
            xMax -= std::round(shiftFactor * xSpan) + 1.0;
            if (xMax <= xMin) xMax = xMin + 1.0;
        }
        xAxis->SetRangeUser(xMin, xMax);
    } else if (axisName == "YMin") {
        if (increase) {
            yMin += shiftFactor * ySpan;
            if (yMin >= yMax) yMin = yMax - 1.0;
        } else {
            yMin -= shiftFactor * ySpan;
            if (isLog && yMin < logFloor) yMin = logFloor;
            else if (!isLog && yMin < 0.0) yMin = 0.0;
        }
        hist->GetYaxis()->SetRangeUser(yMin, yMax);
        hist->SetMinimum(yMin);
        hist->SetMaximum(yMax);
    } else if (axisName == "YMax") {
        if (increase) {
            yMax += shiftFactor * ySpan;
        } else {
            yMax -= shiftFactor * ySpan;
            if (yMax <= yMin) yMax = yMin + 1.0;
        }
        hist->GetYaxis()->SetRangeUser(yMin, yMax);
        hist->SetMinimum(yMin);
        hist->SetMaximum(yMax);
    }

    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

#include <QInputDialog>

//==============================================================================
// QMainCanvas::zoomAroundCursor
//==============================================================================
// Triggered by 'X'. Zooms around the current cursor position, maintaining
// the current zoom width if possible, or defaulting to a window if fully unzoomed.
//==============================================================================
void QMainCanvas::zoomAroundCursor(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;
    
    TAxis *xAxis = hist->GetXaxis();
    int currentMin = xAxis->GetFirst();
    int currentMax = xAxis->GetLast();
    int width = currentMax - currentMin;
    
    if (width >= xAxis->GetNbins() - 2) {
        width = 200; // Default zoom window width if fully unzoomed
    }
    
    int newMin = std::max(1, binX - width / 2);
    int newMax = std::min(xAxis->GetNbins(), binX + width / 2);
    
    xAxis->SetRange(newMin, newMax);
    adjustYAxisToVisibleMax(hist);
    
    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::goToEnergy
//==============================================================================
// Triggered by 'P'. Prompts user for energy, converts to channel using
// active calibration, and zooms around that channel.
//==============================================================================
void QMainCanvas::goToEnergy()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    bool ok;
    double val = QInputDialog::getDouble(this, tr("Go To Energy/Channel"),
                                         tr("Enter energy or channel:"),
                                         0, 0, 100000, 2, &ok);
    if (!ok) return;

    int targetBin = 0;
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    if (trackHist && trackHist->IsCalibrated()) {
        targetBin = trackHist->EnergyToChannel(val);
    } else {
        targetBin = static_cast<int>(std::round(val));
    }
    
    // Validate bounds
    TAxis *xAxis = hist->GetXaxis();
    if (targetBin < 1) targetBin = 1;
    if (targetBin > xAxis->GetNbins()) targetBin = xAxis->GetNbins();

    int currentMin = xAxis->GetFirst();
    int currentMax = xAxis->GetLast();
    int width = currentMax - currentMin;
    
    if (width >= xAxis->GetNbins() - 2) {
        width = 200; // Default zoom window width if fully unzoomed
    }
    
    int newMin = std::max(1, targetBin - width / 2);
    int newMax = std::min(xAxis->GetNbins(), targetBin + width / 2);
    
    xAxis->SetRange(newMin, newMax);
    adjustYAxisToVisibleMax(hist);
    
    ColorTheFrameOfTheHistogram();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}
